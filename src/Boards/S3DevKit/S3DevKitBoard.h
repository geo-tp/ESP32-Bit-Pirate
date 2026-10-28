#pragma once

#ifdef DEVICE_S3DEVKIT

#include "Boards/Common/Views/RgbLedView.h"
#include "Boards/Common/Inputs/DefaultInput.h"
#include "Boards/Common/Serial/BoardHostSerial.h"

class S3DevKitBoard final {
public:
    void initialize();
    IDeviceView& getDeviceView();
    IInput& getDeviceInput();
    IHostSerial& getHostSerial();

private:
    BoardHostSerial hostSerial;
    RgbLedView deviceView{LED_PIN};
    DefaultInput deviceInput;
};

#endif
