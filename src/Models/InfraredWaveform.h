#pragma once

#include <cstdint>
#include <vector>

struct InfraredWaveform {
    std::vector<uint32_t> pulses; // Microseconds, alternating mark/space.
    uint32_t frequencyHz = 38000;
    unsigned dutyPercent = 33;
};
