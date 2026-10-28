#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read/validate the Bit Pirate SDR CDC adapter and save IQ/FFT CSV captures.

Run after selecting USB -> adapters -> SDR Raw RF CDC in the regular firmware.
Requires pyserial only; does not flash firmware or switch the boot mode.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import sys
import time


def request(port, command, timeout=5.0):
    """One request at a time; handle split USB reads and boot banners."""
    expected = command.split()[0].upper()
    port.write(("\n" + command + "\n").encode("ascii"))
    deadline = time.monotonic() + timeout
    pending = bytearray()
    header = None
    rows = []
    while time.monotonic() < deadline:
        pending.extend(port.readline(512))
        if len(pending) > 1024:
            raise RuntimeError("Oversized or unterminated adapter response")
        while b"\n" in pending:
            raw, _, rest = pending.partition(b"\n")
            pending = bytearray(rest)
            line = raw.decode("ascii", errors="replace").strip()
            if not line:
                continue
            if line.startswith("ERR "):
                raise RuntimeError(line)
            if header is None:
                if line.startswith(expected + " "):
                    header = line
                continue
            if line == "END":
                fields = dict(token.split("=", 1) for token in header.split() if "=" in token)
                return {"header": header, "fields": fields, "rows": rows}
            rows.append(line)
            if len(rows) > 256:
                raise RuntimeError("Too many rows in adapter response")
    raise TimeoutError(f"Incomplete response to {command}; reconnect and send INFO")


def validate_capture(response, kind, center_mhz, sample_rate=80000000):
    fields, rows = response["fields"], response["rows"]
    if not response["header"].startswith(kind + " "):
        raise ValueError("Wrong capture type")
    if int(fields["count"]) != 256 or len(rows) != 256:
        raise ValueError("Capture must contain exactly 256 rows")
    if int(fields["fs_hz"]) != sample_rate:
        raise ValueError("Unexpected nominal sample rate")
    if int(fields["center_hz"]) != center_mhz * 1000000:
        raise ValueError("Unexpected center frequency")
    parsed = []
    for index, row in enumerate(rows):
        columns = row.split(",")
        if len(columns) != 2:
            raise ValueError(f"Invalid row {index}: {row}")
        if kind == "IQ":
            i, q = map(int, columns)
            if not (-512 <= i <= 511 and -512 <= q <= 511):
                raise ValueError("IQ value outside signed 10-bit range")
            parsed.append((i, q))
        else:
            offset, power = int(columns[0]), float(columns[1])
            if offset != (index - 128) * (sample_rate // 256) or not math.isfinite(power):
                raise ValueError("Invalid FFT frequency order or non-finite power")
            parsed.append((offset, power))
    return parsed


def validate_capture_counters(before, after, expected):
    capture_delta = (int(after["captures"]) - int(before["captures"])) & 0xFFFFFFFF
    failure_delta = (int(after["failures"]) - int(before["failures"])) & 0xFFFFFFFF
    if capture_delta != expected:
        raise ValueError(f"Expected {expected} successful captures, device counted {capture_delta}")
    if failure_delta:
        raise ValueError(f"Device reported {failure_delta} capture failures")


def save_capture(directory, kind, number, fields, rows):
    path = directory / f"{kind.lower()}-{number:03d}.csv"
    with path.open("w", newline="") as file:
        writer = csv.writer(file)
        if kind == "IQ":
            writer.writerow(("sample", "i", "q"))
            writer.writerows((n, i, q) for n, (i, q) in enumerate(rows))
        else:
            writer.writerow(("frequency_hz", "offset_hz", "relative_dbfs"))
            writer.writerows((int(fields["center_hz"]) + offset, offset, power) for offset, power in rows)
    return path


def run(args):
    import serial

    # Avoid requesting the usual DTR/RTS bootloader reset on opening the port.
    port = serial.Serial(baudrate=115200, timeout=0.2, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(0.3)
        port.reset_input_buffer()
        info = request(port, "INFO")
        print(info["header"])
        if "BPRF1" not in info["header"]:
            raise RuntimeError("Unexpected adapter protocol")
        initial_counters = info["fields"]
        before = request(port, "MEM")
        print(before["header"])
        if info["fields"].get("ready") != "1":
            raise RuntimeError(f"Radio initialization failed: {info['fields'].get('init_error')}")
        if before["fields"].get("heap_ok") != "1":
            raise RuntimeError("Device reported damaged internal heap")
        if args.diagnostics_only:
            return
        args.output.mkdir(parents=True, exist_ok=True)
        report = {"info": info["fields"], "before": before["fields"], "captures": []}
        constant_iq = 0
        for number in range(1, args.captures + 1):
            for kind in ("IQ", "FFT"):
                frame = request(port, f"{kind} {args.center_mhz}")
                rows = validate_capture(frame, kind, args.center_mhz, int(info["fields"]["fs_hz"]))
                path = save_capture(args.output, kind, number, frame["fields"], rows)
                report["captures"].append({"kind": kind, **frame["fields"], "file": str(path)})
                if kind == "IQ":
                    unique = len(set(rows))
                    constant_iq += unique == 1
                    clipped = sum(abs(i) >= 511 or abs(q) >= 511 for i, q in rows)
                    print(f"{path}: {unique} distinct IQ pairs, {clipped}/256 near full scale")
                else:
                    offset, power = max(rows, key=lambda pair: pair[1])
                    peak = (int(frame["fields"]["center_hz"]) + offset) / 1e6
                    print(f"{path}: peak {peak:.6f} MHz, {power:.2f} relative dBFS")
            time.sleep(0.05)
        after = request(port, "MEM")
        report["after"] = after["fields"]
        (args.output / "memory.json").write_text(json.dumps(report, indent=2) + "\n")
        print(after["header"])
        if after["fields"].get("heap_ok") != "1":
            raise RuntimeError("Device reported damaged heap after capture")
        final_info = request(port, "INFO")
        print(final_info["header"])
        validate_capture_counters(initial_counters, final_info["fields"], 2 * args.captures)
        report["after_info"] = final_info["fields"]
        (args.output / "memory.json").write_text(json.dumps(report, indent=2) + "\n")
        delta = int(after["fields"]["internal_free"]) - int(before["fields"]["internal_free"])
        print(f"Internal free-heap change: {delta:+d} bytes; details in {args.output / 'memory.json'}")
        if constant_iq == args.captures:
            raise RuntimeError("Every IQ frame is constant; useful RF reception has not been demonstrated")
        print("Protocol/capture checks passed. A known RF signal is still needed to validate reception.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="CDC port, e.g. /dev/ttyACM0 or COM5")
    parser.add_argument("--center-mhz", type=int, default=2412, help="integer center frequency (100..6000 MHz)")
    parser.add_argument("--captures", type=int, default=3, help="number of separate IQ/FFT pairs (1..1000)")
    parser.add_argument("--output", type=Path, default=Path("sdr-captures"))
    parser.add_argument("--diagnostics-only", action="store_true")
    args = parser.parse_args()
    if not 100 <= args.center_mhz <= 6000:
        parser.error("--center-mhz must be between 100 and 6000")
    if not 1 <= args.captures <= 1000:
        parser.error("--captures must be between 1 and 1000")
    try:
        run(args)
    except (RuntimeError, TimeoutError, ValueError, KeyError, OSError, ImportError) as error:
        print(f"SDR test failed: {error}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
