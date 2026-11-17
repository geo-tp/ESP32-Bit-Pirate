#pragma once

#include <cstdint>
#include <deque>

#include "Interfaces/IUtilityService.h"

class FakeUtilityService final : public IUtilityService {
public:
    mutable std::deque<uint32_t> nowMsValues;
    mutable uint32_t currentNowMs = 0;
    mutable uint32_t sleepMsCalls = 0;
    mutable uint32_t sleepUsCalls = 0;
    mutable uint32_t lastSleepMs = 0;
    mutable uint32_t lastSleepUs = 0;
    bool advanceTimeOnSleep = false;
    bool useMicrosecondClock = false;
    mutable uint32_t currentNowUs = 0;
    uint32_t randomValue = 0;
    int32_t randomRangeValue = 0;

    uint32_t nowMs() const override {
        if (!nowMsValues.empty()) {
            currentNowMs = nowMsValues.front();
            nowMsValues.pop_front();
        }
        return currentNowMs;
    }

    uint32_t nowUs() const override { return useMicrosecondClock ? currentNowUs : currentNowMs * 1000; }

    void sleepMs(uint32_t durationMs) const override {
        ++sleepMsCalls;
        lastSleepMs = durationMs;
        if (advanceTimeOnSleep) currentNowMs += durationMs;
        if (advanceTimeOnSleep) currentNowUs += durationMs * 1000;
    }

    void sleepUs(uint32_t durationUs) const override {
        ++sleepUsCalls;
        lastSleepUs = durationUs;
        if (advanceTimeOnSleep) currentNowMs += durationUs / 1000;
        if (advanceTimeOnSleep) currentNowUs += durationUs;
    }

    uint32_t randomUint32() const override { return randomValue; }

    int32_t randomRange(int32_t, int32_t) const override {
        return randomRangeValue;
    }

    void queueNowMs(uint32_t value) {
        nowMsValues.push_back(value);
    }
};
