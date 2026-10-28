#pragma once

#include <cstdint>
#include <vector>
#include "Interfaces/II2cSnifferService.h"

// Runs are hardware-measured SCL levels, in RMT ticks. No wall-clock or
// callback timestamps enter the frequency calculation.
class I2cFrequencyAnalyzer {
public:
    void addPulse(uint32_t ticks, bool high);
    void endBurst();
    I2cFrequencyResult result(uint32_t resolutionHz);

private:
    struct Cycle {
        uint32_t low;
        uint32_t high;
        uint32_t period() const { return low + high; }
    };
    std::vector<Cycle> cycles;
    uint32_t lowTicks = 0;
    uint32_t highTicks = 0;
};
