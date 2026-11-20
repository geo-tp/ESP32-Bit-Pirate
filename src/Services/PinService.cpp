#include "PinService.h"
#

void PinService::setInput(uint8_t pin) {
    pinMode(pin, INPUT);
    pullState[pin] = NOPULL;
    unmarkActiveOutputPin(pin);
}

void PinService::setInputPullup(uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);
    pullState[pin] = PULL_UP;
    unmarkActiveOutputPin(pin);
}

void PinService::setInputPullDown(uint8_t pin) {
    pinMode(pin, INPUT_PULLDOWN);
    pullState[pin] = PULL_DOWN;
    unmarkActiveOutputPin(pin);
}

void PinService::setOutput(uint8_t pin) {
    pinMode(pin, OUTPUT);
}

void PinService::setHigh(uint8_t pin) {
    setOutput(pin);  // force OUTPUT
    digitalWrite(pin, HIGH);
    markActiveOutputPin(pin);
}

void PinService::setLow(uint8_t pin) {
    setOutput(pin);  // force OUTPUT
    digitalWrite(pin, LOW);
    markActiveOutputPin(pin);
}

bool PinService::read(uint8_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

bool PinService::isInputMode(uint8_t pin) {
    gpio_io_config_t config;
    if (gpio_get_io_config((gpio_num_t)pin, &config) == ESP_OK) {
        return !config.oe;
    } else {
        return false;
    }
}

void PinService::togglePullup(uint8_t pin) {
    pullType enabled = pullState[pin];

    if (enabled == PULL_UP) {
        setInput(pin);
    } else {
        setInputPullup(pin);
    }
}

void PinService::togglePullDown(uint8_t pin) {
    pullType enabled = pullState[pin];

    if (enabled == PULL_DOWN) {
        setInput(pin);
    } else {
        setInputPullDown(pin);
    }
}

int PinService::readAnalog(uint8_t pin) {
    pinMode(pin, INPUT); 
    unmarkActiveOutputPin(pin);
    return analogRead(pin);
}

bool PinService::setupPwm(uint8_t pin, uint32_t freq, uint8_t dutyPercent) {
    if (dutyPercent > 100) dutyPercent = 100;

    detachSignal(pin); // detach if already active

    uint8_t resolutionBits;
    if (freq > 300000) {
        resolutionBits = 6;
    } else if (freq > 150000) {
        resolutionBits = 7;
    } else if (freq > 60000) {
        resolutionBits = 8;
    } else if (freq > 20000) {
        resolutionBits = 9;
    } else {
        resolutionBits = 10;
    }

    if (!ledcAttach(pin, freq, resolutionBits)) {
        return false;
    }

    uint32_t dutyMax = (1UL << resolutionBits) - 1;
    uint32_t dutyVal = (uint32_t(dutyPercent) * dutyMax) / 100U;
    
    bool writeResult = ledcWrite(pin, dutyVal);
    if (writeResult) {
        markActivePwmPin(pin);
    }

    return writeResult;
}

bool PinService::setServoAngle(uint8_t pin, uint8_t angle) {
  const int freq = 50; // Hz
  const int resolution = 14;  // max stable

  detachSignal(pin); // detach if already active

  // setup et attach
  ledcAttach(pin, freq, resolution);

  // period and duty
  const uint32_t periodUs = 1000000UL / freq;    // 20000 us
  const uint32_t dutyMax  = (1UL << resolution) - 1;

  // map angle
  uint32_t pulseUs = map(angle, 0, 180, 1000, 2000);
  uint32_t dutyVal = (pulseUs * dutyMax) / periodUs;

  bool writeResult = ledcWrite(pin, dutyVal);
  if (writeResult) {
      markActivePwmPin(pin);
  }
  
  return writeResult;
}

PinService::pullType PinService::getPullType(uint8_t pin){
    return(pullState[pin]);
}

std::vector<uint8_t> PinService::getConfiguredPullPins() {
    std::vector<uint8_t> pins;
    pins.reserve(pullState.size());

    for (const auto& entry : pullState) {
        pins.push_back(entry.first);
    }

    return pins;
}

void PinService::detachSignal(uint8_t pin) {
    if (!isActivePwmPin(pin)) return;

    ledcDetach(pin);
    unmarkActivePwmPin(pin);
}

bool PinService::isActivePwmPin(uint8_t pin) const {
    return std::find(activePwmPins.begin(), activePwmPins.end(), pin) != activePwmPins.end();
}

void PinService::markActivePwmPin(uint8_t pin) {
    if (!isActivePwmPin(pin)) activePwmPins.push_back(pin);
}

void PinService::unmarkActivePwmPin(uint8_t pin) {
    auto it = std::remove(activePwmPins.begin(), activePwmPins.end(), pin);
    if (it != activePwmPins.end()) {
        activePwmPins.erase(it, activePwmPins.end());
    }
}

bool PinService::isActiveOutputPin(uint8_t pin) const {
    return std::find(activeOutputPins.begin(), activeOutputPins.end(), pin) != activeOutputPins.end();
}

void PinService::markActiveOutputPin(uint8_t pin) {
    if (!isActiveOutputPin(pin)) activeOutputPins.push_back(pin);
}

void PinService::unmarkActiveOutputPin(uint8_t pin) {
    auto it = std::remove(activeOutputPins.begin(), activeOutputPins.end(), pin);
    if (it != activeOutputPins.end()) {
        activeOutputPins.erase(it, activeOutputPins.end());
    }
}

void PinService::releaseOutputPins() {
    // Re-pin every output-driven pin to plain INPUT (high-Z, no pull) —
    // same semantics as the per-pin `reset <gpio>` command.
    for (uint8_t pin : activeOutputPins) {
        setInput(pin);
    }
    activeOutputPins.clear();
}

void PinService::releaseActivePwmPins() {
    // Copy first: detachSignal() unmarks (erases) the pin, so iterating the
    // live vector while it mutates is undefined.
    std::vector<uint8_t> copy = activePwmPins;
    activePwmPins.clear();
    for (uint8_t pin : copy) {
        detachSignal(pin);
    }
}