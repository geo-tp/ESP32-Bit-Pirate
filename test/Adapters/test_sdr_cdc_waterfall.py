# SPDX-License-Identifier: MIT
import io
import struct
import unittest

from sdr_cdc_waterfall import (
    HEADER,
    estimate_usable_bandwidth,
    read_frame,
    read_info_response,
    span_around_center,
    validate_frequency_range,
)


def make_header(frame_type=1, center_mhz=2437, count=3, flags=0, magic=b"BPRF"):
    center = center_mhz * 1000000
    version = 3 if center > 0xFFFFFFFF else 2
    payload_bytes = count * 4 if frame_type == 1 else 0
    header = HEADER.pack(magic, version, frame_type, flags, 19, center & 0xFFFFFFFF,
                         80000000, count, 52, payload_bytes)
    header += struct.pack("<hBBI", -1, 1, 1, 64500000)
    if version == 3:
        header += struct.pack("<I", center >> 32)
    return header


class WaterfallProtocolTest(unittest.TestCase):
    def test_usable_bandwidth_follows_baseline_edges(self):
        offsets = [-40 + 0.3125 * i for i in range(257)]
        flat = [0.0 if abs(o) <= 12 else -30.0 for o in offsets]
        lower, upper = estimate_usable_bandwidth(offsets, flat, 6.0)
        self.assertAlmostEqual(lower, -12, delta=1.0)
        self.assertAlmostEqual(upper, 12, delta=1.0)
        notched = [-20.0 if abs(o) < 0.5 else v for o, v in zip(offsets, flat)]
        lower, upper = estimate_usable_bandwidth(offsets, notched, 6.0)
        self.assertAlmostEqual(lower, -12, delta=1.0)
        self.assertAlmostEqual(upper, 12, delta=1.0)
        wide_lower, wide_upper = estimate_usable_bandwidth(
            offsets, [0.0 if abs(o) <= 12 else -5.0 for o in offsets], 6.0)
        self.assertEqual((wide_lower, wide_upper), (offsets[0], offsets[-1]))

    def test_span_around_center_is_even_and_clamped(self):
        self.assertEqual(span_around_center(2437, 24.4), (2425, 2449))
        self.assertEqual(span_around_center(2437, 0.4), (2436, 2438))
        self.assertEqual(span_around_center(2437, 200), (2397, 2477))
    def test_frequency_range(self):
        self.assertEqual(validate_frequency_range(2200, 2280), (2240, 80))
        self.assertEqual(validate_frequency_range(2400, 2440), (2420, 40))
        for values in ((2198, 2278), (2740, 2820), (2200, 2282), (2400, 2398)):
            with self.assertRaises(ValueError):
                validate_frequency_range(*values)

    def test_consumes_info_terminator_before_binary_mode(self):
        class FakeSerial:
            def __init__(self):
                self.lines = iter([
                    b"READY BPRF1 ready=1\n",
                    b"INFO BPRF1 ready=1 init_error=ESP_OK\n",
                    b"END\n",
                    b"BPRF\x02\x01",
                ])

            def readline(self):
                return next(self.lines)

        port = FakeSerial()
        self.assertEqual(
            read_info_response(port),
            "INFO BPRF1 ready=1 init_error=ESP_OK",
        )
        self.assertEqual(next(port.lines), b"BPRF\x02\x01")

    def test_reads_iq_frame_and_exact_payload(self):
        payload = struct.pack("<III", 0x00080201, 0x00000000, 0x000fffff)
        for center in (100, 2437, 4294, 4295, 6000):
            stream = io.BytesIO(make_header(center_mhz=center) + payload + b"next frame")
            frame = read_frame(stream, 0.1)
            self.assertEqual(frame[0:7], (1, 0, 19, center * 1000000, 80000000, 3, 52))
            self.assertEqual(frame[7], payload)
            self.assertEqual(stream.read(), b"next frame")

    def test_reads_control_frame_without_payload(self):
        for center in (2437, 6000):
            for kind in (2, 3):
                frame = read_frame(io.BytesIO(make_header(kind, center, count=0)), 0.1)
                self.assertEqual(frame[0], kind)
                self.assertEqual(frame[3], center * 1000000)
                self.assertEqual(frame[-1], b"")

    def test_rejects_unknown_magic(self):
        header = make_header(magic=b"NOPE")
        with self.assertRaisesRegex(ValueError, "Invalid BPRF frame"):
            read_frame(io.BytesIO(header), 0.1)

    def test_rejects_old_version_and_unrequested_packing(self):
        legacy = HEADER.pack(b"BPRF", 1, 1, 0, 19, 2437000000, 80000000, 3, 52, 12)
        with self.assertRaisesRegex(ValueError, "Invalid BPRF frame"):
            read_frame(io.BytesIO(legacy), 0.1)
        with self.assertRaisesRegex(ValueError, "RAW32"):
            read_frame(io.BytesIO(make_header(flags=0x100)), 0.1)


if __name__ == "__main__":
    unittest.main()
