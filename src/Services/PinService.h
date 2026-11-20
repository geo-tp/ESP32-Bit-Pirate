#pragma once

#include <Arduino.h>
#include <unordered_map>
#include <vector>
#include "Interfaces/IPinService.h"

class PinService : public IPinService {
public:
    void setInput(uint8_t pin) override;
    void setInputPullup(uint8_t pin) override;
    void setInputPullDown(uint8_t pin) override;
    void setOutput(uint8_t pin) override;
    void setHigh(uint8_t pin) override;
    void setLow(uint8_t pin) override;
    bool read(uint8_t pin) override;
    bool isInputMode(uint8_t pin) override;
    void togglePullup(uint8_t pin) override;
    void togglePullDown(uint8_t pin) override;
    int readAnalog(uint8_t pin) override;
    bool setupPwm(uint8_t pin, uint32_t freq, uint8_t dutyPercent) override;
    bool setServoAngle(uint8_t pin, uint8_t angle) override;
    pullType getPullType(uint8_t pin) override;
    std::vector<uint8_t> getConfiguredPullPins() override;
    void detachSignal(uint8_t pin) override;
    void releaseOutputPins() override;
    void releaseActivePwmPins() override;
private:
    // track pins with pull config 
    std::unordered_map<uint8_t, pullType> pullState; 

    // track pins with active PWM 
    std::vector<uint8_t> activePwmPins;

    // track pins driven as outputs (setHigh/setLow)
    std::vector<uint8_t> activeOutputPins;

    bool isActivePwmPin(uint8_t pin) const;
    void markActivePwmPin(uint8_t pin);
    void unmarkActivePwmPin(uint8_t pin);

    bool isActiveOutputPin(uint8_t pin) const;
    void markActiveOutputPin(uint8_t pin);
    void unmarkActiveOutputPin(uint8_t pin);
};
