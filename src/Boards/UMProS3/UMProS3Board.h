#pragma once

#ifdef DEVICE_UMPROS3

#include "Boards/Common/Views/NoScreenDeviceView.h"
#include "Boards/Common/Inputs/DefaultInput.h"
#include "Boards/Common/Serial/BoardHostSerial.h"

class UMProS3Board final {
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
