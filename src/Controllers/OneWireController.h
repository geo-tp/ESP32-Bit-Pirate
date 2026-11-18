#pragma once

#include <string>
#include <sstream>
#include <iomanip>

#include "Interfaces/IOneWireService.h"
#include "Interfaces/IInput.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IUtilityService.h"
#include "Interfaces/IShell.h"
#include "Models/TerminalCommand.h"
#include "Models/ByteCode.h"
#include "States/GlobalState.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "Shells/HelpShell.h"

class OneWireController {
public:
    // Constructor
    OneWireController(
      ITerminalView& terminalView,
      IInput& terminalInput,
      IUtilityService& utilityService,
      IOneWireService& service,
      ArgTransformer& argTransformer,
      UserInputManager& userInputManager,
      IShell& ibuttonShell,
      IShell& eepromShell,
      HelpShell& helpShell
    );

    // Entry point for handle command
    void handleCommand(const TerminalCommand& cmd);

    // Entry point for handle parsed bytecode instructions
    void handleInstruction(const std::vector<ByteCode>& bytecodes);

    // Ensure initialized/configured
    void ensureConfigured();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    IUtilityService& utilityService;
    IOneWireService& oneWireService;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    IShell& ibuttonShell;
    IShell& eepromShell;
    HelpShell& helpShell;
    GlobalState& state = GlobalState::getInstance();
    bool configured = false;

  // Configure 1-Wire bus parameters
    void handleConfig();

    // Send reset pulse and check device presence
    void handlePing();

    // Raw transfer: reset, master-write given bytes, read read_count bytes
    void handleXfer(const TerminalCommand& command);

    // Scan the bus for all devices
    void handleScan();

    // Listen to bus traffic passively
    void handleSniff();

    // Read a byte or data block from the bus
    void handleRead();

    // Print available 1-Wire commands
    void handleHelp();

    // Read and display a device's ROM ID
    bool handleIdRead();

    // Read device scratchpad memory
    bool handleScratchpadRead();

    // Write raw data to the bus
    void handleWrite(const TerminalCommand& command);

    // Write a ROM ID to a device
    void handleIdWrite(std::vector<uint8_t> idBytes);

    // Interactive iButton shell
    void handleIbutton(const TerminalCommand& command);

    // Write to scratchpad memory
    void handleScratchpadWrite(std::vector<uint8_t> scratchpadBytes);

    // Read temperature sensor
    void handleTemperature();

    // 1Wire EEPROM shell
    void handleEeprom();
};
