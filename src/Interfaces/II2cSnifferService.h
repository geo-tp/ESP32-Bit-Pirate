#pragma once
#include <cstdint>
#include <functional>

struct I2cFrequencyResult {
    bool reliable = false;
    uint32_t capturedCycles = 0;
    uint32_t acceptedCycles = 0;
    double frequencyHz = 0;
    double periodUs = 0;
    double lowUs = 0;
    double highUs = 0;
};

class II2cSnifferService {
public:
    virtual ~II2cSnifferService() = default;
    // Reserve capture/decode RAM on the first sniff; release on exit.
    virtual bool prepare() { return true; } // default keeps existing test fakes compatible
    virtual void begin(uint8_t scl, uint8_t sda) = 0;
    virtual void setAddressFilter(bool enabled, uint8_t address) = 0;
    virtual bool setup() = 0;
    virtual const char* backendName() const = 0;
    virtual const char* lastError() const = 0;
    virtual void stop() = 0;
    virtual void release() = 0;
    virtual bool available() = 0;
    virtual char read() = 0;
    virtual void resetBuffer() = 0;
    virtual bool measureFrequency(uint8_t scl, uint32_t timeoutMs,
                                  I2cFrequencyResult& result,
                                  const std::function<bool()>& shouldStop) = 0;
};
