#pragma once

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IUtilityService.h"
#include "Interfaces/IUsbAdapterShell.h"
#include "Managers/UserInputManager.h"
#include "Services/NvsService.h"
#include "Services/SystemService.h"
#include "States/GlobalState.h"

class UsbAdapterShell : public IUsbAdapterShell {
public:
    UsbAdapterShell(ITerminalView& tv,
                    IInput& in,
                    IUtilityService& utilityService,
                    UserInputManager& uim,
                    NvsService& nvs,
                    SystemService& systemService);

    void run() override;
    void rebootUsbUartBridge();
    void rebootFlashromSerprog();
    void rebootAvrDudeBusPirate();
    void rebootBpio2();
    void rebootSumpLogicAnalyzer();
    void rebootOpenOcdBusPirate() override;
    void rebootInfraredToy();
    void rebootSubGhzRawCdc();
    void rebootSdrCdc();

private:
    void rebootIntoAdapter(const char* title,
                           const char* description,
                           const char* example,
                           const char* webTool = nullptr,
                           const char* returnInstruction = "Reset or press a device button to return to normal mode.");

    ITerminalView& terminalView;
    IInput& terminalInput;
    IUtilityService& utilityService;
    UserInputManager& userInputManager;
    NvsService& nvsService;
    SystemService& systemService;
    GlobalState& state = GlobalState::getInstance();

    inline static constexpr const char* actions[] = {
        " USB-UART bridge",
        " Flashrom serprog",
        " AVRDUDE Bus Pirate SPI",
        " SUMP logic analyzer",
        " OpenOCD JTAG/SWD",
        " USB IR Toy / LIRC",
        " SubGHz CDC CC1101",
        " Bit Bang IO/SPI/I2C",
        " ESP-SDR Raw 2.4GHz",
        " Exit"
    };
    inline static constexpr size_t actionsCount = sizeof(actions) / sizeof(actions[0]);
};
