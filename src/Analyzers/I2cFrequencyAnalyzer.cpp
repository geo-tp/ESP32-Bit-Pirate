#include "I2cFrequencyAnalyzer.h"
#include <algorithm>

void I2cFrequencyAnalyzer::addPulse(uint32_t ticks, bool high) {
    if (ticks == 0) {
        endBurst();
        return;
    }

    if (high) {
        if (lowTicks != 0) highTicks += ticks;
        return;
    }

    if (highTicks != 0) {
        // A following falling edge proves that both preceding runs finished.
        // The last (possibly idle/truncated) run of a capture is never counted.
        cycles.push_back({lowTicks, highTicks});
        lowTicks = highTicks = 0;
    }
    lowTicks += ticks;
}

void I2cFrequencyAnalyzer::endBurst() {
    // Never join pulses across RX restarts, idle timeouts or missing data.
    lowTicks = highTicks = 0;
}

I2cFrequencyResult I2cFrequencyAnalyzer::result(uint32_t resolutionHz) {
    I2cFrequencyResult out;
    out.capturedCycles = cycles.size();
    if (cycles.empty() || resolutionHz == 0) return out;

    std::sort(cycles.begin(), cycles.end(), [](const Cycle& a, const Cycle& b) {
        return a.period() < b.period();
    });

    // Find the most populated period band (10%, or two quantization ticks).
    // Sporadic idle gaps, stretched clocks and glitches fall outside this
    // band instead of biasing the result toward the bus's traffic rate.
    size_t bestStart = 0, bestEnd = 0, end = 0;
    for (size_t start = 0; start < cycles.size(); ++start) {
        const uint32_t period = cycles[start].period();
        const uint32_t tolerance = std::max<uint32_t>(2u, period / 10u);
        while (end < cycles.size() && cycles[end].period() - period <= tolerance) ++end;
        if (end - start > bestEnd - bestStart) {
            bestStart = start;
            bestEnd = end;
        }
    }

    out.acceptedCycles = bestEnd - bestStart;
    // A handful of noise edges or two equally common clock speeds should
    // not produce a confidently labelled bus frequency.
    out.reliable = out.acceptedCycles >= 8 &&
                   out.acceptedCycles * 5u >= out.capturedCycles * 3u;
    if (!out.reliable) return out;

    uint64_t low = 0, high = 0;
    for (size_t i = bestStart; i < bestEnd; ++i) {
        low += cycles[i].low;
        high += cycles[i].high;
    }
    const double ticksPerUs = resolutionHz / 1000000.0;
    out.lowUs = low / (ticksPerUs * out.acceptedCycles);
    out.highUs = high / (ticksPerUs * out.acceptedCycles);
    out.periodUs = out.lowUs + out.highUs;
    out.frequencyHz = 1000000.0 / out.periodUs;
    return out;
}
