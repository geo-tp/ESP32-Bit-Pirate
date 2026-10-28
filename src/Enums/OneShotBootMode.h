#pragma once

#include <cstdint>

// Persisted in NVS: keep existing numeric values compatible with saved modes.
enum class OneShotBootMode : uint8_t {
    None = 0,
    UsbUartBridge = 1,
    FlashromSerprog = 2,
    SumpLogicAnalyzer = 3,
    OpenOcdBusPirate = 4,
    AvrDudeBusPirate = 6,
    InfraredToy = 7,
    SubGhzRawCdc = 8,
    Bpio2 = 9,
    SdrCdc = 10,
};
