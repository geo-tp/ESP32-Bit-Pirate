#pragma once

#include <cstdint>
#include "Interfaces/II2cSnifferService.h"

inline uint32_t fakeI2cSnifferBeginCalls = 0;
inline uint32_t fakeI2cSnifferReleaseCalls = 0;
inline uint8_t fakeI2cSnifferScl = 0;
inline uint8_t fakeI2cSnifferSda = 0;
inline uint32_t fakeI2cSnifferFilterCalls = 0;
inline bool fakeI2cSnifferFilterEnabled = false;
inline uint8_t fakeI2cSnifferFilterAddress = 0;
inline uint8_t fakeI2cSnifferLastEnabledAddress = 0;

class FakeI2cSnifferService final : public II2cSnifferService {
public:
    uint32_t frequencyCalls = 0;
    uint8_t frequencyScl = 0;
    uint32_t frequencyTimeoutMs = 0;
    bool frequencySuccess = true;
    bool pollFrequencyCancellation = false;
    bool frequencyCancelled = false;
    I2cFrequencyResult frequencyResult;
    std::function<void()> onFrequencyMeasure;

    bool measureFrequency(uint8_t scl, uint32_t timeoutMs, I2cFrequencyResult& result,
                          const std::function<bool()>& shouldStop) override {
        ++frequencyCalls;
        frequencyScl = scl;
        frequencyTimeoutMs = timeoutMs;
        if (onFrequencyMeasure) onFrequencyMeasure();
        if (pollFrequencyCancellation) frequencyCancelled = shouldStop();
        result = frequencyResult;
        return frequencySuccess;
    }
    void begin(uint8_t scl, uint8_t sda) override {
        ++fakeI2cSnifferBeginCalls;
        fakeI2cSnifferScl = scl;
        fakeI2cSnifferSda = sda;
    }
    void setAddressFilter(bool enabled, uint8_t address) override {
        ++fakeI2cSnifferFilterCalls;
        fakeI2cSnifferFilterEnabled = enabled;
        fakeI2cSnifferFilterAddress = address;
        if (enabled) fakeI2cSnifferLastEnabledAddress = address;
    }
    bool setup() override { return true; }
    const char* backendName() const override { return "RMT RX"; }
    const char* lastError() const override { return "none"; }
    void stop() override {}
    void release() override { ++fakeI2cSnifferReleaseCalls; }
    bool available() override { return false; }
    char read() override { return '\0'; }
    void resetBuffer() override {}
};

inline void resetFakeI2cSniffer() {
    fakeI2cSnifferBeginCalls = 0;
    fakeI2cSnifferReleaseCalls = 0;
    fakeI2cSnifferScl = 0;
    fakeI2cSnifferSda = 0;
    fakeI2cSnifferFilterCalls = 0;
    fakeI2cSnifferFilterEnabled = false;
    fakeI2cSnifferFilterAddress = 0;
    fakeI2cSnifferLastEnabledAddress = 0;
}
