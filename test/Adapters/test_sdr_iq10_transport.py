# SPDX-License-Identifier: MIT
"""Exercise production SDR packing/framing with short and stalled USB writes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_sdr_bandwidth import ROOT, write_fake_sdk
from test_sdr_bandwidth_protocol import function


@unittest.skipUnless(shutil.which("g++"), "Host C++ compiler unavailable")
class Iq10TransportTest(unittest.TestCase):
    def test_transport(self):
        adapter = (ROOT / "src/Adapters/SdrCdcAdapter.cpp").read_text()
        dsp = adapter[adapter.index("namespace SdrDsp"):adapter.index("constexpr uint32_t internalCaps")]
        parser = adapter[adapter.index("enum class CommandKind"):adapter.index("// Lives on")]
        methods = "\n".join(function(adapter, signature) for signature in (
            "bool reply(", "void handleEvent(", "bool writeBytes(", "bool writePackedIq(",
            "static void putU16(", "static void putU32(", "size_t appendMetadata(",
            "void writeControlFrame(", "bool binaryCapture(", "void stream(",
            "bool processStreamControl(", "void bandwidthState(", "void rateState(",
            "void gainState(", "void snapshot("))
        harness = (ROOT / "test/Adapters/sdr_iq10_transport_harness.inc").read_text()
        harness = harness.replace("// INSERT DSP", dsp).replace("// INSERT PARSER", parser).replace("// INSERT METHODS", methods)
        with tempfile.TemporaryDirectory(prefix="sdr-iq10-") as directory:
            path = Path(directory)
            write_fake_sdk(path)
            (path / "transport.cpp").write_text(harness)
            executable = path / "transport"
            subprocess.run([
                "g++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined,bounds", "-fno-sanitize-recover=all",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "-I" + str(path), "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Services"),
                str(path / "transport.cpp"), str(ROOT / "src/Services/SdrCaptureService.cpp"),
                "-o", str(executable)], check=True)
            subprocess.run([str(executable)], cwd=path, check=True)


if __name__ == "__main__":
    unittest.main()
