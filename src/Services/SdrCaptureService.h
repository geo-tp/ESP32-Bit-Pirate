// SPDX-License-Identifier: MIT
#pragma once

#include <esp_err.h>
#include <cstddef>
#include <cstdint>
#include "Managers/BootMemoryManager.h"

class SdrCaptureService {
public:
    // Terminal IQ/FFT diagnostics use 256 samples. Binary captures can request
    // larger blocks directly from the dedicated RF SRAM bank.
    static constexpr size_t FFT_SAMPLE_COUNT = 256;
    static constexpr uint32_t DEFAULT_SAMPLE_RATE = 80000000;
    static constexpr size_t BANK_BYTES = BootMemoryManager::SDR_BANK_BYTES;
    // Match ESPARGOS S3: leave four words unused at the end of the RF dump bank.
    static constexpr size_t MAX_SAMPLE_COUNT = BANK_BYTES / sizeof(uint32_t) - 4;
    // Experimental PLL requests, matching the reference's integer-MHz range.
    // Useful reception is not implied; the recommended region remains 2.4 GHz.
    static constexpr uint32_t MIN_CENTER_MHZ = 100;
    static constexpr uint32_t MAX_CENTER_MHZ = 6000;
    static_assert(MAX_SAMPLE_COUNT <= 0x7fff, "capture count exceeds register field");

    // AUTO keeps the calibrated filter; 0 selects the widest filter, not bypass.
    static constexpr int AUTO_BANDWIDTH = -1;
    static constexpr unsigned MIN_BANDWIDTH_MHZ = 13;
    static constexpr unsigned MAX_BANDWIDTH_MHZ = 69;
    static constexpr uint8_t UNKNOWN_BANDWIDTH_CODE = 255;

    enum class Status {
        Ok,
        NotReady,
        Frequency,
        Count,
        EngineBusy,
        Timeout,
        Unchanged,
        Filter
    };

    struct BandwidthReading {
        uint8_t iCode = UNKNOWN_BANDWIDTH_CODE;
        uint8_t qCode = UNKNOWN_BANDWIDTH_CODE;
        uint32_t estimatedHz = 0; // Zero means outside the characterized curve / unknown.
    };

    struct Capture {
        Status status;
        esp_err_t error = ESP_OK;
        uint32_t elapsedUs = 0;
        uint32_t control = 0;
        bool retuned = false; // First good capture after tuning/filter/rate/gain change.
        BandwidthReading bandwidth = {};
    };

    // BootMemoryManager must have reserved the RF bank for this boot.
    esp_err_t begin();

    // Receive bandwidth, in approximate MHz.
    static constexpr bool isValidBandwidth(int mhz) {
        return mhz == AUTO_BANDWIDTH || mhz == 0 ||
               (mhz >= int(MIN_BANDWIDTH_MHZ) && mhz <= int(MAX_BANDWIDTH_MHZ));
    }
    bool setBandwidth(int mhz);
    int bandwidthMHz() const { return selectedBandwidthMHz; }
    uint32_t targetBandwidthHz() const; // Quantized estimate; zero in AUTO.
    BandwidthReading lastBandwidth() const { return lastBandwidthReading; }

    // ESPARGOS S3 verified dump clocks: native 80 MS/s, bit15 = 40 MS/s,
    // bit16 = 16 MS/s. These are nominal hardware rates, not resampling.
    bool setSampleRate(uint32_t hz);
    uint32_t sampleRate() const { return selectedSampleRate; }

    // Manual gain uses PHY table indices, not dB. PioArduino's ESP32-S3
    // libphy.a exports phy_force_rx_gain(), although no public header declares it.
    bool manualGainSupported() const;
    unsigned maxGainIndex() const;
    bool setHardwareGain();
    bool setManualGain(unsigned index);
    bool manualGain() const { return gainManual; }
    unsigned gainIndex() const { return selectedGainIndex; }

    // Capture data and current tuning state.
    Capture capture(uint32_t centerFrequencyMHz, size_t sampleCount = FFT_SAMPLE_COUNT);
    const volatile uint32_t* samples() const;

    bool isReady() const { return ready; }
    uint32_t retuneCount() const { return retunes; }
    uint32_t lastRetuneUs() const { return lastRetune; }
    uint32_t activeFrequency() const { return activeFrequencyMHz; }
    uint32_t wifiChannel() const { return wifiChannelSet; }

private:
    bool applyGain();

    bool ready = false;
    bool retunePending = false;
    bool rxReconfigurePending = false;
    bool gainManual = false;
    uint32_t activeFrequencyMHz = 0;
    uint32_t wifiChannelSet = 0;
    uint32_t retunes = 0;
    uint32_t lastRetune = 0;
    uint32_t selectedSampleRate = DEFAULT_SAMPLE_RATE;
    unsigned selectedGainIndex = 40;
    int selectedBandwidthMHz = AUTO_BANDWIDTH;
    BandwidthReading lastBandwidthReading;
};
