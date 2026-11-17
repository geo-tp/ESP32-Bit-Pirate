#include "RgbLedDeviceView.h"

#include <Arduino.h>

RgbLedDeviceView::RgbLedDeviceView(uint8_t pin) : pin(pin) {}

void RgbLedDeviceView::initialize() {
    clear();
}

void RgbLedDeviceView::show(PinoutConfig& config) {
    mode = ModeEnumMapper::fromString(config.getMode());
    write();
}

void RgbLedDeviceView::clear() {
    mode = ModeEnum::HIZ;
    write();
}

void RgbLedDeviceView::setBrightness(uint8_t value) {
    brightness = value;
    write();
}

uint8_t RgbLedDeviceView::getBrightness() {
    return brightness;
}

void RgbLedDeviceView::write() {
    const uint8_t level = brightness;
    uint8_t r = 0, g = 0, b = 0;
    switch (mode) {
        case ModeEnum::HIZ:
        case ModeEnum::None:    break;                          // off
        case ModeEnum::I2C:     b = level; break;               // blue
        case ModeEnum::SPI:     g = level; break;               // green
        case ModeEnum::UART:
        case ModeEnum::HDUART:  r = level; g = level; break;    // yellow
        case ModeEnum::OneWire: g = level; b = level; break;    // cyan
        default:                r = g = b = level / 3; break;   // dim white
    }
    // The core's own RMT driver, not FastLED: LED mode owns FastLED.
    rgbLedWrite(pin, r, g, b);
}
