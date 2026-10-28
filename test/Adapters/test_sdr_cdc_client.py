# SPDX-License-Identifier: MIT
import unittest

from sdr_cdc_test import request, validate_capture, validate_capture_counters


class FakeSerial:
    def __init__(self, chunks):
        self.chunks = iter(chunks)
        self.written = b""

    def write(self, data):
        self.written += data

    def readline(self, size):
        return next(self.chunks, b"")


class ClientTest(unittest.TestCase):
    def test_split_response_and_late_usb_connection(self):
        port = FakeSerial([b"READY BPRF1 ready=1\n", b"IN", b"FO BPRF1 ready=1\r", b"\nEND\n"])
        result = request(port, "INFO")
        self.assertEqual(result["fields"]["ready"], "1")
        self.assertEqual(result["rows"], [])
        self.assertEqual(port.written, b"\nINFO\n")

    def test_device_error_and_truncated_frame(self):
        with self.assertRaisesRegex(RuntimeError, "CAPTURE_TIMEOUT"):
            request(FakeSerial([b"ERR CAPTURE_TIMEOUT control=0\n"]), "IQ 2412")
        with self.assertRaises(TimeoutError):
            request(FakeSerial([b"IQ count=256\n1,2\n"]), "IQ 2412", timeout=0.01)

    def test_sample_ranges_and_fft_order(self):
        fields = {"count": "256", "center_hz": "2412000000", "fs_hz": "80000000"}
        iq = {"header": "IQ ", "fields": fields, "rows": ["-512,511"] * 256}
        self.assertEqual(len(validate_capture(iq, "IQ", 2412)), 256)
        iq["rows"][5] = "512,0"
        with self.assertRaises(ValueError):
            validate_capture(iq, "IQ", 2412)
        fft = {"header": "FFT ", "fields": fields,
               "rows": [f"{(i - 128) * 312500},-25.4" for i in range(256)]}
        self.assertEqual(len(validate_capture(fft, "FFT", 2412)), 256)
        fft["rows"][0] = "-40000000,nan"
        with self.assertRaises(ValueError):
            validate_capture(fft, "FFT", 2412)
        fft["rows"].pop()
        with self.assertRaises(ValueError):
            validate_capture(fft, "FFT", 2412)

    def test_text_capture_above_32bit_hz_at_each_rate(self):
        for sample_rate in (16000000, 40000000, 80000000):
            fields = {"count": "256", "center_hz": "6000000000", "fs_hz": str(sample_rate)}
            iq = {"header": "IQ ", "fields": fields, "rows": ["-512,511"] * 256}
            self.assertEqual(len(validate_capture(iq, "IQ", 6000, sample_rate)), 256)
            fft = {"header": "FFT ", "fields": fields,
                   "rows": [f"{(i - 128) * (sample_rate // 256)},-25.4" for i in range(256)]}
            self.assertEqual(len(validate_capture(fft, "FFT", 6000, sample_rate)), 256)
            with self.assertRaisesRegex(ValueError, "center frequency"):
                validate_capture(fft, "FFT", 4295, sample_rate)

    def test_firmware_capture_counters(self):
        before = {"captures": "8", "failures": "2"}
        after = {"captures": "14", "failures": "2"}
        validate_capture_counters(before, after, 6)
        with self.assertRaisesRegex(ValueError, "capture failures"):
            validate_capture_counters(before, {"captures": "14", "failures": "3"}, 6)
        with self.assertRaisesRegex(ValueError, "successful captures"):
            validate_capture_counters(before, after, 5)


if __name__ == "__main__":
    unittest.main()
