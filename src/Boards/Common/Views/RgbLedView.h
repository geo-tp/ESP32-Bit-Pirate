#pragma once

#include "Boards/Common/Views/NoScreenDeviceView.h"

// A board's built-in RGB LED as a one-pixel display: show() lights it in the
// colour of the current mode. Everything a screen would draw is a no-op,
// inherited from NoScreenDeviceView.
//
// HIZ is dark (every pin released), I2C blue, SPI green, UART/HDUART yellow,
// 1WIRE cyan, any other mode dim white.
class RgbLedView : public NoScreenDeviceView {
public:
    explicit RgbLedView(uint8_t pin);

    void initialize() override;
    void show(PinoutConfig& config) override;
    void clear() override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() override;

private:
    void write();

    uint8_t pin;
    // Low on purpose: a WS2812 at full power is blinding on the desk.
    uint8_t brightness = 24;
    ModeEnum mode = ModeEnum::HIZ;
};
