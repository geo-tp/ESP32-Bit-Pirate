#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Live SDR waterfall for the Bit Pirate binary CDC stream."""
import argparse
import queue
import struct
import sys
import threading
import time

HEADER = struct.Struct("<4sBBHIIIIII")
SAMPLE_RATE = 80_000_000
MAX_SAMPLES = 16_380
SAMPLE_COUNTS = (1024, 2048, 4096, 8192, MAX_SAMPLES)
WATERFALL_ROWS = 320
MIN_CENTER_MHZ = 2200
MAX_CENTER_MHZ = 2800
VIEW_SCALES = {"raw": (-100.0, 0.0), "clean": (-100.0, 0.0), "baseline": (-10.0, 50.0)}


def read_exact(port, length, timeout):
    deadline = time.monotonic() + timeout
    result = bytearray()
    while len(result) < length:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"Timed out reading {length} bytes ({len(result)} received)")
        chunk = port.read(length - len(result))
        if chunk:
            result.extend(chunk)
    return bytes(result)


def read_frame(port, timeout):
    fields = HEADER.unpack(read_exact(port, HEADER.size, timeout))
    magic, version, frame_type, flags, sequence, center_hz, sample_rate, count, capture_us, payload_bytes = fields
    if magic != b"BPRF" or version not in (2, 3):
        raise ValueError(f"Invalid BPRF frame: magic={magic!r}, version={version}")
    # Every current frame carries RX bandwidth; v3 adds the center's high bits.
    read_exact(port, 8, timeout)
    if version == 3:
        center_hz |= struct.unpack("<I", read_exact(port, 4, timeout))[0] << 32
    if payload_bytes > MAX_SAMPLES * 4:
        raise ValueError(f"Frame payload too large: {payload_bytes} bytes")
    if frame_type == 1:
        if flags & ~1 or not 1 <= count <= MAX_SAMPLES or payload_bytes != count * 4:
            raise ValueError("Expected a valid RAW32 IQ frame")
    elif frame_type not in (2, 3) or payload_bytes:
        raise ValueError("Invalid control frame")
    payload = read_exact(port, payload_bytes, timeout) if payload_bytes else b""
    return frame_type, flags, sequence, center_hz, sample_rate, count, capture_us, payload


def read_info_response(port, timeout=5):
    deadline = time.monotonic() + timeout
    info = None
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if line.startswith("ERR "):
            raise RuntimeError(line)
        if line.startswith("INFO BPRF1 "):
            info = line
        elif line == "END" and info is not None:
            return info
    if info is None:
        raise TimeoutError("No INFO BPRF1 response; select SDR mode and check the CDC port")
    raise TimeoutError("Incomplete INFO response; expected END before binary stream")


def validate_frequency_range(start_mhz, end_mhz):
    if not MIN_CENTER_MHZ <= start_mhz < end_mhz <= MAX_CENTER_MHZ:
        raise ValueError(f"Frequency range must be inside {MIN_CENTER_MHZ}..{MAX_CENTER_MHZ} MHz")
    span_mhz = end_mhz - start_mhz
    if span_mhz > SAMPLE_RATE // 1_000_000 or span_mhz % 2:
        raise ValueError("Range width must be an even number of MHz up to 80 MHz")
    return (start_mhz + end_mhz) // 2, span_mhz


def iq_samples(payload, sample_count, np):
    words = np.frombuffer(payload, dtype="<u4")
    if words.size != sample_count:
        raise ValueError("IQ payload length does not match the frame sample count")
    i_values = (words & 0x3FF).astype(np.int16)
    q_values = ((words >> 10) & 0x3FF).astype(np.int16)
    i_values = ((i_values ^ 0x200) - 0x200).astype(np.float32)
    q_values = ((q_values ^ 0x200) - 0x200).astype(np.float32)
    return i_values + 1j * q_values


def spectrum_db(samples, window, np):
    spectrum = np.fft.fftshift(np.fft.fft(samples * window))
    amplitude = np.abs(spectrum) / (float(window.sum()) * 512.0)
    return (20.0 * np.log10(np.maximum(amplitude, 1e-10))).astype(np.float32)


def estimate_usable_bandwidth(offsets_mhz, level_db, threshold_db,
                              reference_mhz=5.0, smooth_bins=5, ignore_mhz=1.0):
    """Return (lower, upper) offsets where the smoothed baseline stays within threshold_db of its central median."""
    count = len(offsets_mhz)
    half = smooth_bins // 2
    smooth = []
    for index in range(count):
        low, high = max(0, index - half), min(count, index + half + 1)
        smooth.append(sum(level_db[low:high]) / (high - low))
    central = sorted(smooth[i] for i in range(count) if abs(offsets_mhz[i]) <= reference_mhz)
    if not central:
        raise ValueError("No bins inside the reference region")
    floor = central[len(central) // 2] - threshold_db
    centre = min(range(count), key=lambda i: abs(offsets_mhz[i]))
    # Bins next to DC are always accepted: DC removal leaves a notch there.
    upper = centre
    while upper + 1 < count and (smooth[upper + 1] >= floor or abs(offsets_mhz[upper + 1]) <= ignore_mhz):
        upper += 1
    lower = centre
    while lower > 0 and (smooth[lower - 1] >= floor or abs(offsets_mhz[lower - 1]) <= ignore_mhz):
        lower -= 1
    return offsets_mhz[lower], offsets_mhz[upper]


def span_around_center(center_mhz, bandwidth_mhz):
    width = min(SAMPLE_RATE // 1_000_000, max(2, 2 * round(bandwidth_mhz / 2)))
    return center_mhz - width // 2, center_mhz + width // 2


class StreamWorker:
    def __init__(self, port_name, start_frequency_mhz, end_frequency_mhz, sample_count, np):
        from PySide6.QtCore import QThread, Signal

        class Worker(QThread):
            status = Signal(str)
            failed = Signal(str)

            def __init__(self, owner):
                super().__init__()
                self.owner = owner

            def run(self):
                self.owner.run_stream(self)

        self.worker = Worker(self)
        self.port_name = port_name
        self.start_frequency_mhz = start_frequency_mhz
        self.end_frequency_mhz = end_frequency_mhz
        self.center_frequency_mhz, self.span_mhz = validate_frequency_range(
            start_frequency_mhz, end_frequency_mhz
        )
        self.sample_count = sample_count
        self.np = np
        self.offsets_mhz = np.fft.fftshift(
            np.fft.fftfreq(sample_count, d=1.0 / SAMPLE_RATE)
        ).astype(np.float32) / 1_000_000
        self.baseline_lock = threading.Lock()
        self.baseline_target = 0
        self.baseline_count = 0
        self.baseline_sum = None
        self.baseline_db = None
        self.data = queue.Queue(maxsize=12)
        self.stop_requested = threading.Event()
        self.port_lock = threading.Lock()
        self.port = None
        self.stream_started = False
        self.stop_sent = False
        self.dropped_frames = 0
        self.received_frames = 0
        self.stream_started_at = None

    def run_stream(self, worker):
        try:
            import serial

            port = serial.Serial()
            port.port = self.port_name
            port.baudrate = 115200
            port.timeout = 0.2
            port.write_timeout = 2
            port.dtr = True
            port.rts = False
            port.open()
            with self.port_lock:
                self.port = port

            port.reset_input_buffer()
            port.write(b"\nINFO\n")
            info = read_info_response(port)
            if "ready=1" not in info:
                raise RuntimeError(f"SDR radio is not ready: {info}")
            worker.status.emit(info)

            window = self.np.hanning(self.sample_count).astype(self.np.float32)
            port.write(f"STREAM2 {self.center_frequency_mhz} {self.sample_count} RAW32\n".encode("ascii"))
            self.stream_started_at = time.monotonic()
            with self.port_lock:
                self.stream_started = True
                if self.stop_requested.is_set():
                    self._send_stop_locked()

            previous_sequence = None
            while True:
                frame_type, flags, sequence, center_hz, sample_rate, count, capture_us, payload = read_frame(
                    port, timeout=8
                )
                if frame_type == 2:
                    break
                if frame_type == 3:
                    raise RuntimeError(f"Adapter error frame: code {flags}")
                if frame_type != 1:
                    raise ValueError(f"Unknown frame type {frame_type}")
                if sample_rate != SAMPLE_RATE or count != self.sample_count or len(payload) != count * 4:
                    raise ValueError("Unexpected IQ frame sample rate or length")
                expected_center = self.center_frequency_mhz * 1_000_000
                if center_hz != expected_center:
                    raise ValueError(f"Unexpected center frequency {center_hz} Hz")
                if previous_sequence is not None and sequence != ((previous_sequence + 1) & 0xFFFFFFFF):
                    raise ValueError("Capture sequence discontinuity")
                previous_sequence = sequence
                samples = iq_samples(payload, count, self.np)
                raw_db = spectrum_db(samples, window, self.np)
                clean_db = spectrum_db(samples - samples.mean(), window, self.np)
                self.accumulate_baseline(clean_db)
                self.received_frames += 1
                item = (raw_db, clean_db, center_hz, capture_us, sequence)
                try:
                    self.data.put_nowait(item)
                except queue.Full:
                    try:
                        self.data.get_nowait()
                    except queue.Empty:
                        pass
                    self.dropped_frames += 1
                    self.data.put_nowait(item)
            worker.status.emit("Stream stopped")
        except Exception as error:
            worker.failed.emit(str(error))
        finally:
            with self.port_lock:
                port = self.port
                self.port = None
            if port is not None:
                port.close()

    def begin_baseline(self, frames):
        with self.baseline_lock:
            self.baseline_count = 0
            self.baseline_sum = None
            self.baseline_db = None
            self.baseline_target = frames

    def accumulate_baseline(self, clean_db):
        with self.baseline_lock:
            if not self.baseline_target:
                return
            # Average in linear power: averaging dB values would bias the noise floor low.
            linear = 10.0 ** (clean_db.astype(self.np.float64) / 10.0)
            self.baseline_sum = linear if self.baseline_sum is None else self.baseline_sum + linear
            self.baseline_count += 1
            if self.baseline_count >= self.baseline_target:
                mean_power = self.baseline_sum / self.baseline_count
                self.baseline_db = (10.0 * self.np.log10(mean_power)).astype(self.np.float32)
                self.baseline_target = 0

    def _send_stop_locked(self):
        if self.port is not None and self.stream_started and not self.stop_sent:
            self.port.write(b"STOP\n")
            self.stop_sent = True

    def request_stop(self):
        self.stop_requested.set()
        with self.port_lock:
            self._send_stop_locked()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port; omit to choose it in the window")
    parser.add_argument("--range-start-mhz", type=int, default=2397,
                        help="lower edge of the selected RF range (MHz)")
    parser.add_argument("--range-end-mhz", type=int, default=2477,
                        help="upper edge of the selected RF range (MHz; maximum 80 MHz span)")
    parser.add_argument("--samples", type=int, choices=SAMPLE_COUNTS, default=4096)
    parser.add_argument("--baseline-frames", type=int, default=64,
                        help="captures averaged for the baseline reference (8..1024; default: 64)")
    parser.add_argument("--edge-db", type=float, default=6.0,
                        help="drop from the central baseline level that defines the usable band edge (dB)")
    args = parser.parse_args()
    if not 8 <= args.baseline_frames <= 1024:
        parser.error("--baseline-frames must be between 8 and 1024")
    try:
        center_frequency_mhz, span_mhz = validate_frequency_range(
            args.range_start_mhz, args.range_end_mhz
        )
    except ValueError as error:
        parser.error(str(error))

    try:
        import numpy as np
        import pyqtgraph as pg
        from PySide6 import QtCore, QtGui, QtWidgets
        from serial.tools import list_ports
    except ImportError as error:
        print("Install the viewer dependencies with: python3 -m pip install pyserial numpy pyqtgraph PySide6", file=sys.stderr)
        raise SystemExit(1) from error

    pg.setConfigOptions(background="#101820", foreground="#d5e0e8", antialias=False, imageAxisOrder="row-major")

    class WaterfallWindow(QtWidgets.QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle("Bit Pirate SDR | Live Waterfall")
            self.resize(1100, 760)
            self.worker = None
            self.waterfall = np.zeros((WATERFALL_ROWS, 0), dtype=np.uint8)
            self.displayed_frames = 0
            self.rate_started = time.monotonic()
            self.last_sequence = 0
            self.baseline_db = None
            self.baseline_edges = None
            self.active_view = None

            central = QtWidgets.QWidget()
            layout = QtWidgets.QVBoxLayout(central)
            controls = QtWidgets.QHBoxLayout()
            layout.addLayout(controls)

            controls.addWidget(QtWidgets.QLabel("CDC"))
            self.port_select = QtWidgets.QComboBox()
            self.port_select.setEditable(True)
            self.refresh_ports()
            if args.port:
                self.port_select.setCurrentText(args.port)
            controls.addWidget(self.port_select, 2)
            refresh_button = QtWidgets.QPushButton("Refresh")
            refresh_button.clicked.connect(self.refresh_ports)
            controls.addWidget(refresh_button)

            controls.addWidget(QtWidgets.QLabel("Range MHz"))
            self.range_start = QtWidgets.QSpinBox()
            self.range_start.setRange(MIN_CENTER_MHZ, MAX_CENTER_MHZ - 2)
            self.range_start.setValue(args.range_start_mhz)
            controls.addWidget(self.range_start)
            controls.addWidget(QtWidgets.QLabel("to"))
            self.range_end = QtWidgets.QSpinBox()
            self.range_end.setRange(MIN_CENTER_MHZ + 2, MAX_CENTER_MHZ)
            self.range_end.setValue(args.range_end_mhz)
            controls.addWidget(self.range_end)

            controls.addWidget(QtWidgets.QLabel("Samples"))
            self.sample_select = QtWidgets.QComboBox()
            for count in SAMPLE_COUNTS:
                self.sample_select.addItem(f"{count:,}", count)
            self.sample_select.setCurrentIndex(SAMPLE_COUNTS.index(args.samples))
            controls.addWidget(self.sample_select)

            self.start_button = QtWidgets.QPushButton("Start")
            self.start_button.clicked.connect(self.start_stream)
            controls.addWidget(self.start_button)
            self.stop_button = QtWidgets.QPushButton("Stop")
            self.stop_button.setEnabled(False)
            self.stop_button.clicked.connect(self.stop_stream)
            controls.addWidget(self.stop_button)

            analysis_controls = QtWidgets.QHBoxLayout()
            layout.addLayout(analysis_controls)
            analysis_controls.addWidget(QtWidgets.QLabel("View"))
            self.view_select = QtWidgets.QComboBox()
            self.view_select.addItem("Raw (no processing)", "raw")
            self.view_select.addItem("DC removed", "clean")
            self.view_select.addItem("DC removed - baseline (after calibration)", "baseline")
            self.view_select.setCurrentIndex(1)
            analysis_controls.addWidget(self.view_select)
            self.raw_overlay = QtWidgets.QCheckBox("Overlay raw")
            self.raw_overlay.setChecked(True)
            analysis_controls.addWidget(self.raw_overlay)
            analysis_controls.addWidget(QtWidgets.QLabel("Baseline captures"))
            self.baseline_frames = QtWidgets.QSpinBox()
            self.baseline_frames.setRange(8, 1024)
            self.baseline_frames.setValue(args.baseline_frames)
            analysis_controls.addWidget(self.baseline_frames)
            self.calibrate_button = QtWidgets.QPushButton("Calibrate baseline")
            self.calibrate_button.setEnabled(False)
            self.calibrate_button.clicked.connect(self.start_baseline)
            analysis_controls.addWidget(self.calibrate_button)
            analysis_controls.addWidget(QtWidgets.QLabel("Edge threshold dB"))
            self.edge_db = QtWidgets.QDoubleSpinBox()
            self.edge_db.setRange(1.0, 30.0)
            self.edge_db.setSingleStep(0.5)
            self.edge_db.setValue(args.edge_db)
            analysis_controls.addWidget(self.edge_db)
            self.apply_span_button = QtWidgets.QPushButton("Apply derived span")
            self.apply_span_button.setEnabled(False)
            self.apply_span_button.clicked.connect(self.apply_derived_span)
            analysis_controls.addWidget(self.apply_span_button)
            analysis_controls.addStretch(1)

            self.waterfall_plot = pg.PlotWidget()
            self.waterfall_plot.setLabel("bottom", "Frequency", units="MHz")
            self.waterfall_plot.setLabel("left", "Recent captures")
            self.waterfall_plot.setMenuEnabled(False)
            self.waterfall_plot.showGrid(x=True, y=False, alpha=0.16)
            self.waterfall_plot.setXRange(args.range_start_mhz, args.range_end_mhz, padding=0)
            self.image = pg.ImageItem(axisOrder="row-major")
            colors = np.array([
                [4, 12, 34], [10, 60, 150], [0, 180, 210], [250, 220, 45], [245, 70, 30]
            ], dtype=np.ubyte)
            self.image.setLookupTable(pg.ColorMap(np.linspace(0, 1, len(colors)), colors).getLookupTable(0, 1, 256))
            self.waterfall_plot.addItem(self.image)
            self.waterfall_plot.setYRange(0, WATERFALL_ROWS, padding=0)
            layout.addWidget(self.waterfall_plot, 3)

            self.spectrum_plot = pg.PlotWidget()
            self.spectrum_plot.setLabel("bottom", "Frequency", units="MHz")
            self.spectrum_plot.setLabel("left", "Relative level", units="dBFS")
            self.spectrum_plot.setMenuEnabled(False)
            self.spectrum_plot.showGrid(x=True, y=True, alpha=0.16)
            self.spectrum_plot.setYRange(-100, 0, padding=0)
            self.spectrum_plot.setXRange(args.range_start_mhz, args.range_end_mhz, padding=0)
            self.raw_line = self.spectrum_plot.plot(pen=pg.mkPen("#6b7b88", width=1))
            self.spectrum_line = self.spectrum_plot.plot(pen=pg.mkPen("#52d6c8", width=1.3))
            self.edge_lines = []
            for _ in range(2):
                edge = pg.InfiniteLine(angle=90, pen=pg.mkPen("#f2c14e", style=QtCore.Qt.PenStyle.DashLine))
                edge.hide()
                self.spectrum_plot.addItem(edge)
                self.edge_lines.append(edge)
            layout.addWidget(self.spectrum_plot, 1)

            self.status = QtWidgets.QLabel("Select the SDR CDC port and start a stream")
            self.status.setStyleSheet("color: #a7bac8; padding: 4px 2px;")
            layout.addWidget(self.status)
            self.analysis = QtWidgets.QLabel("")
            self.analysis.setStyleSheet("color: #f2c14e; padding: 0 2px 4px 2px;")
            layout.addWidget(self.analysis)
            self.setCentralWidget(central)

            self.range_start.valueChanged.connect(self.refresh_axes)
            self.range_end.valueChanged.connect(self.refresh_axes)
            self.view_select.currentIndexChanged.connect(self.reset_waterfall)
            self.edge_db.valueChanged.connect(self.update_baseline_analysis)

            self.timer = QtCore.QTimer(self)
            self.timer.timeout.connect(self.update_plots)
            self.timer.start(33)

        def refresh_axes(self):
            start, end = self.range_start.value(), self.range_end.value()
            if end > start:
                self.waterfall_plot.setXRange(start, end, padding=0)
                self.spectrum_plot.setXRange(start, end, padding=0)
                self.reset_waterfall()

        def reset_waterfall(self):
            self.waterfall = np.zeros((WATERFALL_ROWS, 0), dtype=np.uint8)

        def apply_view_axes(self, view):
            self.active_view = view
            floor_db, ceiling_db = VIEW_SCALES[view]
            self.spectrum_plot.setYRange(floor_db, ceiling_db, padding=0)
            if view == "baseline":
                self.spectrum_plot.setLabel("left", "Level above baseline", units="dB")
            else:
                self.spectrum_plot.setLabel("left", "Relative level", units="dBFS")
            self.reset_waterfall()

        def clear_baseline(self):
            self.baseline_db = None
            self.baseline_edges = None
            self.apply_span_button.setEnabled(False)
            for edge in self.edge_lines:
                edge.hide()
            self.analysis.setText("")

        def start_baseline(self):
            if self.worker and self.worker.worker.isRunning():
                self.clear_baseline()
                self.worker.begin_baseline(self.baseline_frames.value())
                self.analysis.setText("Collecting baseline - remove the antenna for a noise-only reference")

        def poll_baseline(self):
            worker = self.worker
            if worker.baseline_target:
                self.analysis.setText(
                    f"Collecting baseline {worker.baseline_count}/{worker.baseline_target} captures"
                )
                return
            result = worker.baseline_db
            if result is not None and result is not self.baseline_db:
                self.baseline_db = result
                self.update_baseline_analysis()

        def update_baseline_analysis(self):
            if self.baseline_db is None or not self.worker:
                return
            threshold = self.edge_db.value()
            try:
                lower, upper = estimate_usable_bandwidth(
                    self.worker.offsets_mhz.tolist(), self.baseline_db.tolist(), threshold
                )
            except ValueError as error:
                self.analysis.setText(str(error))
                return
            self.baseline_edges = (lower, upper)
            center_mhz = self.worker.center_frequency_mhz
            for edge, offset in zip(self.edge_lines, (lower, upper)):
                edge.setPos(center_mhz + offset)
                edge.show()
            self.apply_span_button.setEnabled(True)
            self.analysis.setText(
                f"Baseline of {self.worker.baseline_count} captures | usable RX bandwidth "
                f"{upper - lower:.1f} MHz ({lower:+.1f} to {upper:+.1f} MHz) within {threshold:.1f} dB "
                f"of the central median"
            )

        def apply_derived_span(self):
            if self.baseline_edges is None or not self.worker:
                return
            lower, upper = self.baseline_edges
            start, end = span_around_center(self.worker.center_frequency_mhz, upper - lower)
            try:
                validate_frequency_range(start, end)
            except ValueError as error:
                self.status.setText(str(error))
                return
            self.range_start.setValue(start)
            self.range_end.setValue(end)

        def view_level(self, view, raw_db, clean_db):
            if view == "raw":
                return raw_db
            if view == "clean":
                return clean_db
            return clean_db - self.baseline_db

        def refresh_ports(self):
            current = self.port_select.currentText() if hasattr(self, "port_select") else args.port or ""
            ports = [item.device for item in list_ports.comports()]
            self.port_select.clear()
            self.port_select.addItems(ports)
            if current:
                self.port_select.setCurrentText(current)

        def start_stream(self):
            port_name = self.port_select.currentText().strip()
            if not port_name:
                self.status.setText("Choose a CDC serial port first")
                return
            self.status.setText(f"Opening {port_name} and checking SDR adapter...")
            start_frequency_mhz = self.range_start.value()
            end_frequency_mhz = self.range_end.value()
            try:
                center_frequency_mhz, _ = validate_frequency_range(
                    start_frequency_mhz, end_frequency_mhz
                )
            except ValueError as error:
                self.status.setText(str(error))
                return
            sample_count = int(self.sample_select.currentData())
            self.waterfall = np.zeros((WATERFALL_ROWS, 0), dtype=np.uint8)
            self.clear_baseline()
            self.waterfall_plot.setXRange(start_frequency_mhz, end_frequency_mhz, padding=0)
            self.spectrum_plot.setXRange(start_frequency_mhz, end_frequency_mhz, padding=0)
            self.worker = StreamWorker(port_name, start_frequency_mhz, end_frequency_mhz, sample_count, np)
            self.worker.worker.status.connect(self.status.setText)
            self.worker.worker.failed.connect(self.show_error)
            self.worker.worker.finished.connect(self.stream_finished)
            self.worker.worker.start()
            self.start_button.setEnabled(False)
            self.stop_button.setEnabled(True)
            self.port_select.setEnabled(False)
            self.calibrate_button.setEnabled(True)
            self.sample_select.setEnabled(False)
            self.displayed_frames = 0
            self.rate_started = time.monotonic()

        def stop_stream(self):
            if self.worker:
                self.worker.request_stop()
                self.stop_button.setEnabled(False)
                self.status.setText("Stopping after the current block...")

        def stream_finished(self):
            self.start_button.setEnabled(True)
            self.stop_button.setEnabled(False)
            self.port_select.setEnabled(True)
            self.calibrate_button.setEnabled(False)
            self.sample_select.setEnabled(True)

        def show_error(self, message):
            self.status.setText(f"CDC error: {message}")

        def update_plots(self):
            if not self.worker:
                return
            self.poll_baseline()
            center_mhz = self.worker.center_frequency_mhz
            offsets = self.worker.offsets_mhz
            visible = ((offsets >= self.range_start.value() - center_mhz) &
                       (offsets < self.range_end.value() - center_mhz))
            if not visible.any():
                return
            view = self.view_select.currentData()
            if view == "baseline" and self.baseline_db is None:
                view = "clean"
            if view != self.active_view:
                self.apply_view_axes(view)
            floor_db, ceiling_db = VIEW_SCALES[view]
            latest = None
            while True:
                try:
                    latest = self.worker.data.get_nowait()
                except queue.Empty:
                    break
                raw_db, clean_db, center_hz, capture_us, sequence = latest
                level = self.view_level(view, raw_db, clean_db)
                intensity = np.clip(
                    (level[visible] - floor_db) * 255.0 / (ceiling_db - floor_db), 0, 255
                ).astype(np.uint8)
                if self.waterfall.shape[1] != intensity.size:
                    self.waterfall = np.zeros((WATERFALL_ROWS, intensity.size), dtype=np.uint8)
                self.waterfall[:-1] = self.waterfall[1:]
                self.waterfall[-1] = intensity
                self.displayed_frames += 1
                self.last_sequence = sequence
            if latest is None:
                return
            x_mhz = center_mhz + offsets[visible]
            self.spectrum_line.setData(x_mhz, level[visible])
            if self.raw_overlay.isChecked() and view == "clean":
                self.raw_line.setData(x_mhz, raw_db[visible])
            else:
                self.raw_line.clear()
            start_mhz = self.range_start.value()
            end_mhz = self.range_end.value()
            self.image.setRect(QtCore.QRectF(start_mhz, 0, end_mhz - start_mhz, WATERFALL_ROWS))
            self.image.setImage(self.waterfall, autoLevels=False, levels=(0, 255))
            start_time = self.worker.stream_started_at or self.rate_started
            elapsed = max(time.monotonic() - start_time, 1e-3)
            sample_rate = self.worker.received_frames * self.worker.sample_count / elapsed
            dropped = self.worker.dropped_frames
            self.status.setText(
                f"{self.displayed_frames} displayed  |  {sample_rate:,.0f} IQ/s received  |  "
                f"capture {capture_us} us  |  dropped for display {dropped}  |  seq {self.last_sequence}"
            )

        def closeEvent(self, event):
            if self.worker and self.worker.worker.isRunning():
                self.worker.request_stop()
                self.worker.worker.wait(2500)
            event.accept()

    app = QtWidgets.QApplication(sys.argv[:1])
    window = WaterfallWindow()
    window.show()
    raise SystemExit(app.exec())


if __name__ == "__main__":
    main()
