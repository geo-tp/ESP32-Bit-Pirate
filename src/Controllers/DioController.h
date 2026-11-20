// DioController.h
#pragma once

#include <string>
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IDeviceView.h"
#include "Interfaces/IUtilityService.h"
#include "Interfaces/IPinService.h"
#include "Models/TerminalCommand.h"
#include "Models/PinoutConfig.h"
#include "States/GlobalState.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "Shells/HelpShell.h"

class DioController {
public:
    // Constructor
    DioController(ITerminalView& terminalView, IInput& terminalInput, IDeviceView& deviceView, IUtilityService& utilityService, IPinService& pinService, ArgTransformer& argTransformer, HelpShell& helpShell, UserInputManager& userInputManager);

    // Entry point to handle a DIO command
    void handleCommand(const TerminalCommand& cmd);

    // Release DIO resources (called on mode exit): stop PWM and return
    // output-driven pins to INPUT (high-Z)
    void release();

    // Build GPIO and pull configuration lines
    std::vector<std::string> buildPullConfigLines();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    IDeviceView& deviceView;
    IUtilityService& utilityService;
    IPinService& pinService;
    ArgTransformer& argTransformer;
    HelpShell& helpShell;
    UserInputManager& userInputManager;
    GlobalState& state = GlobalState::getInstance();

    // Read digital value from a pin
    void handleReadPin(const TerminalCommand& cmd);

    // Set pin high or low, input or output
    void handleSetPin(const TerminalCommand& cmd);

    // Scan fo rpin changfes and display edge count and rate
    void handleScan(const TerminalCommand& cmd);

    // Display pin configuration and status
    void handlePins(const TerminalCommand& cmd);

    // Enable internal pull-up resistor
    void handlePullup(const TerminalCommand& cmd);

    // Enable internal pull-down resistor
    void handlePulldown(const TerminalCommand& cmd);

    // Start pin state sniffing
    void handleSniff(const TerminalCommand& cmd);

    // Configure PWM on a pin
    void handlePwm(const TerminalCommand& cmd);

    // Reset pin to default state
    void handleResetPin(const TerminalCommand& cmd);

    // Toggle pin state every ms
    void handleTogglePin(const TerminalCommand& cmd);

    // Edge count on a pin
    void handleMeasure(const TerminalCommand& cmd);

    // Set servo angle
    void handleServo(const TerminalCommand& cmd);

    // Send a given ms pulse to a pin
    void handlePulse(const TerminalCommand& cmd);

    // Jam pin with random high/low states
    void handleJamPin(const TerminalCommand& cmd);

    // Display DIO help info
    void handleHelp();

    // Check protected pin
    bool isPinAllowed(uint8_t pin, const std::string& context);
};
