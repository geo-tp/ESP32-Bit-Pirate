#pragma once

#ifdef DEVICE_S3DEVKIT

#include "Boards/Common/Views/NoScreenDeviceView.h"
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
    NoScreenDeviceView deviceView;
    DefaultInput deviceInput;
};

#endif
