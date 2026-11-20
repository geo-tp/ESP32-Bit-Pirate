#pragma once

#include <cstdint>
#include <vector>

class IPinService {
public:
    enum pullType { NOPULL = 0, PULL_UP, PULL_DOWN };

    virtual ~IPinService() = default;

    virtual void setInput(uint8_t pin) = 0;
    virtual void setInputPullup(uint8_t pin) = 0;
    virtual void setInputPullDown(uint8_t pin) = 0;
    virtual void setOutput(uint8_t pin) = 0;
    virtual void setHigh(uint8_t pin) = 0;
    virtual void setLow(uint8_t pin) = 0;
    virtual bool read(uint8_t pin) = 0;
    virtual bool isInputMode(uint8_t pin) = 0;
    virtual void togglePullup(uint8_t pin) = 0;
    virtual void togglePullDown(uint8_t pin) = 0;
    virtual int readAnalog(uint8_t pin) = 0;
    virtual bool setupPwm(uint8_t pin, uint32_t freq, uint8_t dutyPercent) = 0;
    virtual bool setServoAngle(uint8_t pin, uint8_t angle) = 0;
    virtual pullType getPullType(uint8_t pin) = 0;
    virtual std::vector<uint8_t> getConfiguredPullPins() = 0;
    virtual void detachSignal(uint8_t pin) = 0;
    virtual void releaseOutputPins() = 0;
    virtual void releaseActivePwmPins() = 0;
};
