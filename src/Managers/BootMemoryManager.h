#pragma once

#include <cstddef>
#include <cstdint>
#include <esp_err.h>
#include "Enums/OneShotBootMode.h"

class BootMemoryManager {
public:
    static constexpr uintptr_t SDR_BANK_START = 0x3fcd0000;
    static constexpr size_t SDR_BANK_BYTES = 65536;

    // Call first in setup(), after Arduino initializes NVS and before board or
    // service initialization. Reads the pending mode without consuming it and
    // restores one contiguous heap, or two heaps around SDR's private bank.
    static esp_err_t initialize();

    // Check the subsequently consumed boot mode without changing heap ownership.
    static esp_err_t validateBootMode(OneShotBootMode mode);

    // Capture services may use the fixed RF bank only while this returns true.
    static bool isSdrBankReserved();
};
