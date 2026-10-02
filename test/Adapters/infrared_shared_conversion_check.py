"""Run with python test/Adapters/infrared_shared_conversion_check.py (requires g++)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/Services/M5StickS3InfraredService.cpp").read_text()
receive_raw = source[source.index("bool M5StickS3InfraredService::receiveRaw("):source.rindex("#endif")]

# Compile the actual receiveRaw method with only the hardware calls stubbed.
harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include "Transformers/SubGhzTransformer.h"
using rmt_data_t = rmt_symbol_word_t;
constexpr uint32_t MICROS_PER_TICK = 50;
bool rmtReceiveCompleted(uint8_t) { return true; }
uint32_t micros() { return 100000; }
struct InfraredService { static constexpr uint32_t IR_DEFAULT_FREQUENCY_KHZ = 38; };
struct M5StickS3InfraredService {
    bool receiving = true;
    uint8_t rxPin = 0;
    std::array<rmt_data_t, 100> rxSymbols{};
    size_t rxSymbolCount = 0;
    uint32_t lastFrameEndUs = 0;
    uint16_t initialGapTicks = 0;
    unsigned armed = 0;
    bool armReceiver() { ++armed; return true; }
    bool receiveRaw(std::vector<uint16_t>&, uint32_t&);
};
'''
harness += receive_raw
harness += r'''
int main() {
    M5StickS3InfraredService service;
    std::vector<uint16_t> timings{123};
    uint32_t khz = 0;
    auto check = [&](std::vector<rmt_data_t> symbols, std::vector<uint16_t> expected) {
        std::copy(symbols.begin(), symbols.end(), service.rxSymbols.begin());
        service.rxSymbolCount = symbols.size();
        const unsigned armed = service.armed;
        assert(service.receiveRaw(timings, khz) == !expected.empty());
        assert(timings == expected);
        assert(service.armed == armed + 1);
        if (!expected.empty()) assert(khz == 38);
    };
    std::vector<rmt_data_t> nec{{20000, 1, 9000, 0}, {4500, 1, 560, 0}};
    std::vector<uint16_t> expected{9000, 4500, 560};
    const uint32_t code = 0xCB34FF00;
    for (unsigned i = 0; i < 32; ++i) {
        const uint32_t space = code & (1UL << i) ? 1690 : 560;
        nec.push_back({space, 1, 560, 0});
        expected.push_back(space);
        expected.push_back(560);
    }
    nec.push_back({20000, 1, 0, 0});
    check(nec, expected);
    check({{400, 0, 160, 0}, {560, 1, 560, 0}, {0, 1, 9000, 0}}, {560, 560, 560});
    check({{560, 0, 0, 1}, {9000, 0, 4500, 1}}, {560});
    check({{20000, 1, 0, 0}}, {});
    check({{32767, 0, 32767, 0}, {2, 0, 0, 1}}, {});
    check({}, {});
    service.rxSymbolCount = service.rxSymbols.size();
    timings = {123};
    assert(!service.receiveRaw(timings, khz) && timings.empty());
}
'''

with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "check.cpp"
    exe = Path(directory) / "check.exe"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-I", str(root / "test"), "-I", str(root / "src"),
                    str(cpp), str(root / "src/Transformers/SubGhzTransformer.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("Infrared shared conversion checks passed")
