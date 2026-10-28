# SPDX-License-Identifier: MIT
"""Exercise production command parsing, stream control and bandwidth framing."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_sdr_bandwidth import ROOT, write_fake_sdk


def function(source, signature):
    start = source.index(signature)
    # Ignore brace-initialized default arguments before the function body.
    first = start + re.search(r"\)\s*(?:const\s*)?\{", source[start:]).end() - 1
    depth = 1
    end = first + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


@unittest.skipUnless(shutil.which("g++"), "Host C++ compiler unavailable")
class BandwidthProtocolTest(unittest.TestCase):
    def test_commands_stream_changes_and_frame_versions(self):
        adapter = (ROOT / "src/Adapters/SdrCdcAdapter.cpp").read_text()
        parser = adapter[adapter.index("enum class CommandKind"):adapter.index("// Lives on")]
        methods = "\n".join(function(adapter, signature) for signature in (
            "bool reply(", "static void putU16(", "static void putU32(",
            "size_t appendMetadata(", "void writeControlFrame(",
            "bool processStreamControl(", "void bandwidthState("))
        harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define private public
#include "SdrCaptureService.h"
#undef private
uint32_t test_reg_read(uintptr_t reg) {
    assert(reg == 0x6001c02c); // S3 AGCPWR_CTRL7, used to query the gain limit.
    return 82u << 8;
}
uint32_t millis() { static uint32_t now = 0; return ++now; }
void delay(unsigned) {}
struct { void restart() { assert(false); } } ESP;
constexpr size_t bandwidthHeaderBytes = 40, extendedHeaderBytes = 44;
struct Serial {
    std::string input;
    size_t position = 0;
    std::vector<uint8_t> output;
    void queue(const std::string& text) { input = text; position = 0; output.clear(); }
    int available() { return input.size() - position; }
    int read() { return available() ? input[position++] : -1; }
    int availableForWrite() { return 4096; }
    size_t write(const uint8_t* data, size_t length) {
        output.insert(output.end(), data, data + length); return length;
    }
};
''' + parser + r'''
struct Session {
    Serial serial;
    SdrCaptureService radio;
    CommandReader reader;
    uint32_t captures = 7;
    void pollButton() {}
    bool writeBytes(const uint8_t* bytes, size_t size) { return serial.write(bytes, size) == size; }
''' + methods + r'''
};
unsigned u16(const uint8_t* p) { return p[0] | unsigned(p[1]) << 8; }
uint32_t u32(const uint8_t* p) { return u16(p) | uint32_t(u16(p + 2)) << 16; }
int main() {
    for (unsigned mhz = 13; mhz <= 69; ++mhz) {
        auto command = parseCommand(("BANDWIDTH " + std::to_string(mhz)).c_str());
        assert(command.kind == CommandKind::Bandwidth && command.tuningValue == mhz);
    }
    for (const char* value : {"BANDWIDTH 0", " bandwidth wide\t", "BANDWIDTH WIDE"}) {
        auto c = parseCommand(value);
        assert(c.kind == CommandKind::Bandwidth && c.tuningValue == 0);
    }
    assert(parseCommand("bandwidth auto").tuningValue == 65535);
    assert(parseCommand("BANDWIDTH?").kind == CommandKind::BandwidthQuery);
    for (const char* value : {"BANDWIDTH", "BANDWIDTH -1", "BANDWIDTH 12", "BANDWIDTH 70",
         "BANDWIDTH 25.5", "BANDWIDTH WIDE junk", "BANDWIDTH 25 junk", "BANDWIDTH? 1",
         "BANDWIDTH 99999999999999999", "BANDWIDTH 1"})
        assert(parseCommand(value).kind == CommandKind::Invalid);
    assert(parseCommand("RAW 2437 256").kind == CommandKind::Invalid);
    assert(parseCommand("STREAM 2437 256").kind == CommandKind::Invalid);
    assert(parseCommand("RAW2 2437 256").kind == CommandKind::Raw2);
    assert(parseCommand(("STREAM2 2437 " + std::to_string(SdrCaptureService::MAX_SAMPLE_COUNT)).c_str())
           .kind == CommandKind::Stream2);
    assert(parseCommand(("STREAM2 2437 " + std::to_string(SdrCaptureService::MAX_SAMPLE_COUNT + 1)).c_str())
           .kind == CommandKind::Invalid);

    Session session;
    session.writeControlFrame(2, 0, 2437, 0, 0);
    auto& out = session.serial.output;
    assert(out.size() == 40 && out[4] == 2 && out[5] == 2);
    assert(u32(out.data() + 12) == 2437000000u && u32(out.data() + 16) == 80000000u);
    out.clear();
    session.writeControlFrame(3, 10, 2437, 256, 0);
    assert(out.size() == 40 && out[4] == 2 && out[5] == 3 && u16(out.data() + 6) == 10);
    assert(u16(out.data() + 32) == 65535 && out[34] == 255 && out[35] == 255);
    assert(u32(out.data() + 28) == 0 && u32(out.data() + 36) == 0);

    uint16_t center = 2437;
    session.serial.queue("BANDWIDTH 65\nTUNE 2438\nSTOP\n");
    assert(!session.processStreamControl(center, 256));
    assert(center == 2438 && session.radio.bandwidthMHz() == 65 && out.empty());
    assert(session.radio.retunePending);
    uint8_t header[40] = {'B', 'P', 'R', 'F', 2, 1};
    assert(session.appendMetadata(header, 2438, {1, 1, 64500000}) == 40);
    assert(header[4] == 2 && u16(header + 32) == 65 && header[34] == 1 && header[35] == 1);
    assert(u32(header + 36) == 64500000);
    session.serial.queue("");
    session.bandwidthState();
    std::string text(out.begin(), out.end());
    assert(text.find("requested_mhz=65 target_hz=64500000 estimated_hz=0") != std::string::npos);
    assert(text.find("actual_valid=0 approximate=1\nEND\n") != std::string::npos);
    session.serial.queue("BANDWIDTH AUTO\nSTOP\n");
    assert(!session.processStreamControl(center, 256) && session.radio.bandwidthMHz() == -1 && out.empty());
    session.serial.queue("BANDWIDTH WIDE\nSTOP\n");
    assert(!session.processStreamControl(center, 256) && session.radio.bandwidthMHz() == 0 && out.empty());
    session.serial.queue("BANDWIDTH 12\nSTOP\n");
    assert(!session.processStreamControl(center, 256) && session.radio.bandwidthMHz() == 0);
    assert(out.size() == 40 && out[5] == 3 && u16(out.data() + 6) == 7);
    session.serial.queue("BANDWIDTH 25\nSTOP\n");
    assert(!session.processStreamControl(center, 256) && session.radio.bandwidthMHz() == 25);
    assert(out.empty());
    session.serial.queue("STOP\n");
    assert(!session.processStreamControl(center, 256) && out.empty());
}
'''
        with tempfile.TemporaryDirectory(prefix="sdr-bw-protocol-") as directory:
            path = Path(directory)
            write_fake_sdk(path)
            (path / "protocol.cpp").write_text(harness)
            executable = path / "protocol"
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            # Link the real configuration methods; discard unused hardware paths.
                            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                            "-I" + str(path), "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Services"),
                            str(path / "protocol.cpp"), str(ROOT / "src/Services/SdrCaptureService.cpp"),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], cwd=path, check=True)


if __name__ == "__main__":
    unittest.main()
