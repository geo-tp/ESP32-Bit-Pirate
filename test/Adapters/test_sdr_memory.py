# SPDX-License-Identifier: MIT
"""Check boot heap ownership using production code and ESP32-S3 linker addresses."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from test_sdr_bandwidth import ROOT, write_fake_sdk


@unittest.skipUnless(shutil.which("g++") and sys.platform == "linux", "Needs Linux and g++")
class SdrMemoryTest(unittest.TestCase):
    def test_boot_regions_and_ownership(self):
        with tempfile.TemporaryDirectory(prefix="sdr-memory-") as directory:
            path = Path(directory)
            write_fake_sdk(path)
            for heap_start, iram_end, valid in (
                (0x3FCB4D38, 0x4038DC00, True),  # s3-devkit ELF
                (0x3FCB5780, 0x4038DD00, True),  # cardputer-adv ELF
                (0x3FCD0000, 0x4038DC00, False),  # no room below RF bank
                (0x3FCD0004, 0x4038DC00, False),  # static data overlaps bank
                (0x3FCB4D38, 0x403A4D3C, False),  # IRAM alias overlaps pool
            ):
                executable = path / "memory"
                subprocess.run([
                    "g++", "-x", "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fno-pie", "-no-pie", "-ffunction-sections", "-fdata-sections",
                    "-Wl,--gc-sections",
                    f"-Wl,--defsym,_heap_start={heap_start:#x}",
                    f"-Wl,--defsym,_iram_end={iram_end:#x}",
                    "-I" + str(path), "-I" + str(ROOT / "src"),
                    str(ROOT / "test/Adapters/sdr_memory_harness.inc"),
                    str(ROOT / "src/Services/SdrCaptureService.cpp"),
                    "-o", str(executable),
                ], check=True)
                cases = [(mode, 0, 0) for mode in (0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 255)]
                if valid:
                    cases += [(0, 1, 0), (10, 1, 0), (10, 2, 0)]
                    cases += [(10, 0, scenario) for scenario in (1, 2, 3, 4)]
                for mode, fail_call, nvs_scenario in cases:
                    with self.subTest(heap_start=hex(heap_start), iram_end=hex(iram_end),
                                      mode=mode, fail_call=fail_call, nvs_scenario=nvs_scenario):
                        subprocess.run([str(executable), str(mode), str(fail_call), str(int(valid)),
                                        str(nvs_scenario)],
                                       check=True)


if __name__ == "__main__":
    unittest.main()
