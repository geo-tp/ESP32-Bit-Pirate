# SPDX-License-Identifier: MIT
"""Run the production capture preamble against a host PHY model, without RF hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("g++"), "Host C++ compiler unavailable")
class SdrTuningTest(unittest.TestCase):
    def test_exact_frequency_transitions_and_driver_errors(self):
        source = (ROOT / "src/Services/SdrCaptureService.cpp").read_text()
        # Exercise the actual validation, retune cache and RX preparation. Stop
        # before the fixed-address SRAM writes; this does not emulate acquisition.
        start = source.index("SdrCaptureService::Capture SdrCaptureService::capture(")
        end = source.index("    auto* words =", start)
        preamble = source[start:end] + "    return {Status::Ok};\n}\n"
        harness = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>
#define private public
#include "SdrCaptureService.h"
#undef private

static bool sdrBankReserved = true;
bool BootMemoryManager::isSdrBankReserved() { return sdrBankReserved; }
static constexpr uintptr_t captureRegister = 0x60033d5c;
static uint32_t control = 0, frequency = 0, expected = 0;
static uint32_t calibrations = 0, pllWrites = 0;
static esp_err_t driverError = ESP_OK;
static bool rxEnabled = false;
static std::vector<char> calls;
static const std::vector<unsigned> channels = {
    2412, 2417, 2422, 2427, 2432, 2437, 2442,
    2447, 2452, 2457, 2462, 2467, 2472, 2484
};

uint32_t REG_READ(uintptr_t reg) { assert(reg == captureRegister); return control; }
int64_t esp_timer_get_time() { static int64_t now = 0; return ++now; }
constexpr unsigned WIFI_SECOND_CHAN_NONE = 0;
esp_err_t esp_wifi_set_channel(unsigned channel, unsigned second) {
    assert(channel >= 1 && channel <= 13 && second == 0);
    return driverError;
}
void set_chanfreq(unsigned mhz, unsigned second) {
    // The channel calibration API is deliberately restricted to channel MHz.
    assert(std::find(channels.begin(), channels.end(), mhz) != channels.end());
    assert(second == 0);
    frequency = mhz;
    ++calibrations;
    calls.push_back('c');
}
void set_rf_freq_offset(unsigned crystal, unsigned mhz, int khz) {
    assert(crystal == 0 && khz == 0);
    assert(!calls.empty() && calls.back() == 'c' && frequency == 2412);
    frequency = mhz;
    ++pllWrites;
    calls.push_back('f');
}
void stop_tx_tone(unsigned mode) { assert(mode == 1); calls.push_back('s'); }
void rom_pbus_workmode() { calls.push_back('p'); }
void rom_pbus_xpd_tx_off() { calls.push_back('t'); }
void rom_pbus_xpd_rx_on(unsigned enabled) { assert(enabled == 1); calls.push_back('r'); }
void rom_set_rxclk_en(unsigned enabled) {
    assert(enabled == 1 && frequency == expected);
    rxEnabled = true;
    calls.push_back('k');
}
bool SdrCaptureService::applyGain() { return true; }
''' + preamble + r'''
int main() {
    SdrCaptureService radio;
    assert(radio.capture(2412).status == SdrCaptureService::Status::NotReady);
    radio.ready = true;
    radio.wifiChannelSet = 1;
    auto tune = [&](unsigned mhz) {
        expected = mhz;
        const bool changed = radio.activeFrequency() != mhz;
        const auto before = radio.retuneCount();
        calls.clear();
        rxEnabled = false;
        assert(radio.capture(mhz).status == SdrCaptureService::Status::Ok);
        assert(frequency == mhz && radio.activeFrequency() == mhz);
        assert(radio.retuneCount() == before + changed);
        if (changed) {
            const bool channel = std::find(channels.begin(), channels.end(), mhz) != channels.end();
            const std::vector<char> sequence = channel
                ? std::vector<char>{'c', 's', 'p', 't', 'r', 'k'}
                : std::vector<char>{'c', 'f', 's', 'p', 't', 'r', 'k'};
            assert(calls == sequence && rxEnabled && radio.retunePending);
        } else {
            assert(calls.empty() && !rxEnabled);
        }
    };
    assert(radio.MIN_CENTER_MHZ == 100 && radio.MAX_CENTER_MHZ == 6000);
    // All 5901 integer-MHz settings, ascending and descending, plus cache hits.
    for (unsigned f = radio.MIN_CENTER_MHZ; f <= radio.MAX_CENTER_MHZ; ++f) {
        tune(f);
        tune(f);
    }
    for (unsigned f = radio.MAX_CENTER_MHZ; f >= radio.MIN_CENTER_MHZ; --f) tune(f);
    for (unsigned f : {2437u, 2438u, 2300u, 2412u, 2484u, 2485u}) tune(f);

    const auto before = radio.retuneCount();
    calls.clear();
    assert(radio.capture(radio.MIN_CENTER_MHZ - 1).status == SdrCaptureService::Status::Frequency);
    assert(radio.capture(radio.MAX_CENTER_MHZ + 1).status == SdrCaptureService::Status::Frequency);
    assert(radio.capture(2437, 0).status == SdrCaptureService::Status::Count);
    control = 1;
    assert(radio.capture(2437).status == SdrCaptureService::Status::EngineBusy);
    control = 0;
    sdrBankReserved = false;
    assert(radio.capture(2437).status == SdrCaptureService::Status::NotReady);
    sdrBankReserved = true;
    driverError = ESP_FAIL;
    auto failed = radio.capture(2437);
    assert(failed.status == SdrCaptureService::Status::Frequency && failed.error == ESP_FAIL);
    assert(radio.activeFrequency() == 2485 && radio.wifiChannel() == 1);
    assert(radio.retuneCount() == before && calls.empty());
    driverError = ESP_OK;
    tune(2437); // A failed driver change must be retryable.
    assert(radio.wifiChannel() == 6 && calibrations > 0 && pllWrites > 0);
    driverError = ESP_FAIL;
    calls.clear();
    failed = radio.capture(6000); // Out-of-band requests still propagate driver errors.
    assert(failed.status == SdrCaptureService::Status::Frequency && failed.error == ESP_FAIL);
    assert(radio.activeFrequency() == 2437 && radio.wifiChannel() == 6 && calls.empty());
    driverError = ESP_OK;
    tune(6000);
    tune(100);
    tune(2412);
}
'''
        with tempfile.TemporaryDirectory(prefix="sdr-tuning-") as directory:
            path = Path(directory)
            (path / "esp_err.h").write_text(
                "#pragma once\nusing esp_err_t = int;\n"
                "constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 0x102,\n"
                "    ESP_ERR_NOT_SUPPORTED = 0x106;\n"
            )
            (path / "tuning.cpp").write_text(harness)
            executable = path / "tuning"
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(path), "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Services"),
                str(path / "tuning.cpp"), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
