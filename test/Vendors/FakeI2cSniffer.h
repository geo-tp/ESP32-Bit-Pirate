#pragma once

#include <cstdint>

inline uint32_t fakeI2cSnifferBeginCalls = 0;
inline uint32_t fakeI2cSnifferReleaseCalls = 0;
inline uint8_t fakeI2cSnifferScl = 0;
inline uint8_t fakeI2cSnifferSda = 0;
inline uint32_t fakeI2cSnifferFilterCalls = 0;
inline bool fakeI2cSnifferFilterEnabled = false;
inline uint8_t fakeI2cSnifferFilterAddress = 0;
inline uint8_t fakeI2cSnifferLastEnabledAddress = 0;

extern "C" {
inline void i2c_sniffer_begin(uint8_t scl, uint8_t sda) {
    ++fakeI2cSnifferBeginCalls;
    fakeI2cSnifferScl = scl;
    fakeI2cSnifferSda = sda;
}
inline void i2c_sniffer_set_address_filter(bool enabled, uint8_t address) {
    ++fakeI2cSnifferFilterCalls;
    fakeI2cSnifferFilterEnabled = enabled;
    fakeI2cSnifferFilterAddress = address;
    if (enabled) fakeI2cSnifferLastEnabledAddress = address;
}
inline bool i2c_sniffer_setup() { return true; }
inline void i2c_sniffer_stop() {}
inline void i2c_sniffer_release() { ++fakeI2cSnifferReleaseCalls; }
inline bool i2c_sniffer_available() { return false; }
inline char i2c_sniffer_read() { return '\0'; }
inline void i2c_sniffer_reset_buffer() {}
}

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
