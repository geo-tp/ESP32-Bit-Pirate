#ifdef DEVICE_UMPROS3

#include "Boards/UMProS3/UMProS3Board.h"

static constexpr uint8_t LDO2_ENABLE_PIN = 17;

void UMProS3Board::initialize() {
    // Enable LDO2 on the UM ProS3.
    // GPIO17 HIGH = LDO2 enabled.
    pinMode(LDO2_ENABLE_PIN, OUTPUT);
    digitalWrite(LDO2_ENABLE_PIN, HIGH);

    // IO10 - reserved as an input for vbat detection
    // IO33 - reserved as an input for USB power detection

    deviceView.initialize();
}

IDeviceView& UMProS3Board::getDeviceView() {
    return deviceView;
}

IInput& UMProS3Board::getDeviceInput() {
    return deviceInput;
}

IHostSerial& UMProS3Board::getHostSerial() {
    return hostSerial;
}

#endif
