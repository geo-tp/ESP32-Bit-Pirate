#include "ActionDispatcher.h"

/*
Constructor
*/
ActionDispatcher::ActionDispatcher(DependencyProvider& provider)
    : provider(provider) {}

/*
Setup
*/
void ActionDispatcher::setup(TerminalTypeEnum terminalType, std::string terminalInfos) {
    provider.getDeviceView().initialize();
    provider.getDeviceView().welcome(terminalType, terminalInfos);

    if (terminalType == TerminalTypeEnum::SerialPort) {
        provider.getSystemService().setDebugOutput(false); // no serial debug logs
        provider.getTerminalView().initialize();
        provider.getTerminalView().waitPress();
        provider.getCommandLineManager().waitPress();
        provider.getTerminalView().welcome(terminalType, terminalInfos);
    } else {
        provider.getTerminalView().initialize();
        provider.getTerminalView().welcome(terminalType, terminalInfos);
    }
}

/*
Run loop
*/
void ActionDispatcher::run() {
    while (true) {
        const auto mode =
            ModeEnumMapper::toString(state.getCurrentMode());

        provider.getTerminalView().printPrompt(mode);

        const std::string action =
            provider.getCommandLineManager().readCommand(mode);

        if (!action.empty()) {
            dispatch(action);
        }
    }
}

/*
Dispatch
*/
void ActionDispatcher::dispatch(const std::string& raw) {
    if (raw.empty()) return;

    // Alias (or return raw if no alias)
    const std::string& finalRaw = provider.getAliasManager().expand(raw);

    // Instructions
    if (provider.getInstructionTransformer().isInstructionCommand(finalRaw)) {
        auto instructions = provider.getInstructionTransformer().transform(finalRaw);
        dispatchInstructions(instructions);
        return;
    }

    // Macros
    if (provider.getCommandTransformer().isMacroCommand(finalRaw)) {
        provider.getTerminalView().println("Macros Not Yet Implemented.");
        return;
    }

    // Repeated command
    if (provider.getCommandTransformer().isRepeatCommand(finalRaw)) {
        dispatchRepeatCommands(finalRaw);
        return;
    }

    // Pipeline terminal commands
    if (provider.getCommandTransformer().isPipelineCommand(finalRaw)) {
        dispatchPipelineCommands(finalRaw);
        return;
    }

    // Single terminal command
    TerminalCommand cmd = provider.getCommandTransformer().transform(finalRaw);
    dispatchCommand(cmd);
}

/*
Dispatch Command
*/
void ActionDispatcher::dispatchCommand(const TerminalCommand& cmd) {
    // Mode change command
    if (cmd.getRoot() == "mode" || cmd.getRoot() == "m") {
        ModeEnum maybeNewMode = provider.getUtilityController().handleModeChangeCommand(cmd);
        if (maybeNewMode != ModeEnum::None) {
            setCurrentMode(maybeNewMode);
        }
        return;
    }

    // Global command (help, logic, mode, P, p...)
    if (provider.getCommandTransformer().isGlobalCommand(cmd)) {
        provider.getUtilityController().handleCommand(cmd);
        if (provider.getCommandTransformer().isScreenCommand(cmd)) {
            // hack to rerender the pinout view after screen related cmd
            setCurrentMode(state.getCurrentMode());
        } 
        return;
    }

    // Mode specific command
    switch (state.getCurrentMode()) {
        case ModeEnum::HIZ:
            provider.getTerminalView().println("Type 'help' or 'mode'");
            break;
        case ModeEnum::OneWire:
            provider.getOneWireController().handleCommand(cmd);
            break;
        case ModeEnum::UART:
            provider.getUartController().handleCommand(cmd);
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().handleCommand(cmd);
            break;
        case ModeEnum::I2C:
            provider.getI2cController().handleCommand(cmd);
            break;
        case ModeEnum::SPI:
            provider.getSpiController().handleCommand(cmd);
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().handleCommand(cmd);
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().handleCommand(cmd);
            break;
        case ModeEnum::DIO:
            provider.getDioController().handleCommand(cmd);
            break;
        case ModeEnum::LED:
            provider.getLedController().handleCommand(cmd);
            break;
        case ModeEnum::Infrared:
            provider.getInfraredController().handleCommand(cmd);
            break;
        case ModeEnum::USB:
            provider.getUsbController().handleCommand(cmd);
            break;
        case ModeEnum::Bluetooth:
            provider.getBluetoothController().handleCommand(cmd);
            break;
        case ModeEnum::WiFi:
            provider.getWifiController().handleCommand(cmd);
            // Rerender pinout view after WiFi commands
            setCurrentMode(state.getCurrentMode());
            break;
        case ModeEnum::JTAG:
            provider.getJtagController().handleCommand(cmd);
            break;
        case ModeEnum::I2S:
            provider.getI2sController().handleCommand(cmd);
            break;
        case ModeEnum::CAN_:
            provider.getCanController().handleCommand(cmd);
            break;
        case ModeEnum::ETHERNET:
            provider.getEthernetController().handleCommand(cmd);
            break;
        case ModeEnum::SUBGHZ:
            provider.getSubGhzController().handleCommand(cmd);
            break;
        case ModeEnum::RFID:
            provider.getRfidController().handleCommand(cmd);
            break;
        case ModeEnum::RF24_:
            provider.getRf24Controller().handleCommand(cmd);
            break;
        case ModeEnum::FM:
            provider.getFmController().handleCommand(cmd);
            break;
        case ModeEnum::CELL:
            provider.getCellController().handleCommand(cmd);
            break;
        case ModeEnum::LORA:
            provider.getLoRaController().handleCommand(cmd);
            break;
        case ModeEnum::EXPANDER:
            provider.getExpanderController().handleCommand(cmd);
            break;
    }

   // Handled in specific mode, we need to rerender the pinout view
   if (provider.getCommandTransformer().isScreenCommand(cmd)) {
       setCurrentMode(state.getCurrentMode());
   }
}

/*
Dispatch command from repeat
*/
void ActionDispatcher::dispatchRepeatCommands(const std::string& raw) {
    auto cmds = provider.getCommandTransformer().transformRepeatCommand(raw);

    if (cmds.empty()) {
        provider.getTerminalView().println("\nUsage: repeat <count> <cmd>");
        provider.getTerminalView().println("       repeat <count> <cmd> || <cmd> ...");
        provider.getTerminalView().println("Max allowed is 100 repetitions.\n");
        return;
    }

    for (const auto& cmd : cmds) {
        dispatchCommand(cmd);
    }
}

/*
Dispatch command from pipeline
*/
void ActionDispatcher::dispatchPipelineCommands(const std::string& raw) {
    auto cmds = provider.getCommandTransformer().transformMany(raw);

    if (cmds.empty()) {
        provider.getTerminalView().println("Usage: <cmd> || <cmd> || <cmd>");
        return;
    }

    for (const auto& cmd : cmds) {
        // handle nested repeat commands in pipeline
        if (provider.getCommandTransformer().isRepeatCommand(cmd.getRoot())) {
            auto repeatCmdRaw = cmd.getRoot() + " " + cmd.getSubcommand() + " " + cmd.getArgs();
            dispatchRepeatCommands(repeatCmdRaw);
            continue;
        }
        dispatchCommand(cmd);
    }
}

/*
Dispatch Instructions
*/
void ActionDispatcher::dispatchInstructions(const std::vector<Instruction>& instructions) {
    // Convert raw instructions into bytecodes vector
    auto bytecodes = provider.getInstructionTransformer().transformByteCodes(instructions);

    switch (state.getCurrentMode()) {
        case ModeEnum::OneWire:
            provider.getOneWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::UART:
            provider.getUartController().handleInstruction(bytecodes);
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().handleInstruction(bytecodes);
            break;
        case ModeEnum::I2C:
            provider.getI2cController().handleInstruction(bytecodes);
            break;
        case ModeEnum::SPI:
            provider.getSpiController().handleInstruction(bytecodes);
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::LED:
            provider.getLedController().handleInstruction(bytecodes);
            break;
        default:
            provider.getTerminalView().println("Cannot execute instruction in this mode.");
            return;
    }

    // Line by line bytecode
    provider.getTerminalView().println("");
    provider.getTerminalView().println("ByteCode Sequence:");
    for (const auto& code : bytecodes) {
        provider.getTerminalView().println(
            ByteCodeEnumMapper::toString(code.getCommand()) +
            " | data=" + std::to_string(code.getData()) +
            " | bits=" + std::to_string(code.getBits()) +
            " | repeat=" + std::to_string(code.getRepeat())
        );
    }
    provider.getTerminalView().println("");
}


/*
Set Mode
*/
void ActionDispatcher::setCurrentMode(ModeEnum newMode) {
    // Release resources of current mode if needed
    auto currentMode = state.getCurrentMode();
    releaseMode(currentMode, newMode);
    state.setCurrentMode(newMode);

    // Ensure configuration for new mode
    switch (newMode) {
        case ModeEnum::HIZ:
            provider.disableAllProtocols();
            break;
        case ModeEnum::OneWire:
            provider.getOneWireController().ensureConfigured();
            break;
        case ModeEnum::UART:
            provider.getUartController().ensureConfigured();
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().ensureConfigured();
            break;
        case ModeEnum::I2C:
            provider.getI2cController().ensureConfigured();
            break;
        case ModeEnum::SPI:
            provider.getSpiController().ensureConfigured();
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().ensureConfigured();
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().ensureConfigured();
            break;
        case ModeEnum::LED:
            provider.getLedController().ensureConfigured();
            break;
        case ModeEnum::Infrared:
            provider.getInfraredController().ensureConfigured();
            break;
        case ModeEnum::USB:
            provider.getUsbController().ensureConfigured();
            break;
        case ModeEnum::Bluetooth:
            provider.getBluetoothController().ensureConfigured();
            break;
        case ModeEnum::WiFi:
            provider.getWifiController().ensureConfigured();
            break;
        case ModeEnum::JTAG:
            provider.getJtagController().ensureConfigured();
            break;
        case ModeEnum::I2S:
            provider.getI2sController().ensureConfigured();
            break;
        case ModeEnum::CAN_:
            provider.getCanController().ensureConfigured();
            break;
        case ModeEnum::ETHERNET:
            provider.getEthernetController().ensureConfigured();
            break;
        case ModeEnum::SUBGHZ:
            provider.getSubGhzController().ensureConfigured();
            break;
        case ModeEnum::RFID:
            provider.getRfidController().ensureConfigured();
            break;
        case ModeEnum::RF24_:
            provider.getRf24Controller().ensureConfigured();
            break;
        case ModeEnum::FM:
            provider.getFmController().ensureConfigured();
            break;
        case ModeEnum::CELL:
            provider.getCellController().ensureConfigured();
            break;
        case ModeEnum::LORA:
            provider.getLoRaController().ensureConfigured();
            break;
        case ModeEnum::EXPANDER:
             // This will bridge to a UART session with the expander
            provider.getExpanderController().ensureConfigured();
            state.setCurrentMode(ModeEnum::HIZ); // return to HIZ after the uart bridge
            newMode = ModeEnum::HIZ; // to display hiz just after
            break;
        default:
            break;
    }

    // Build pinout view for the screen
    PinoutConfig config = provider.getPinoutTransformer().build(newMode);

    // No pinout for these modes, show some other infos
    if (newMode == ModeEnum::DIO) {
        config.setMappings(provider.getDioController().buildPullConfigLines());
    } else if (newMode == ModeEnum::WiFi) {
        config.setMappings(provider.getWifiController().buildWiFiLines());
    }

    // Show pinout for the mode
    provider.getDeviceView().show(config);
}

/*
Release mode resources
*/
void ActionDispatcher::releaseMode(ModeEnum currentMode, ModeEnum newMode) {
    if (currentMode == newMode) {
        return;
    }

    switch (currentMode) {
        case ModeEnum::Bluetooth:
            // Bluetooth stack is heavy and must be explicitly released
            provider.getBluetoothController().ensureReleased();
            break;

        case ModeEnum::I2C:
            provider.getI2cController().ensureReleased();
            break;

        case ModeEnum::TwoWire:
            provider.getTwoWireController().ensureReleased();
            break;
        case ModeEnum::LORA:
            provider.getLoRaController().ensureReleased();
            break;

        case ModeEnum::DIO:
            provider.getDioController().release();
            break;

        case ModeEnum::OneWire:
            provider.getOneWireController().release();
            break;

        case ModeEnum::UART:
            provider.getUartController().release();
            break;

        // For now, no realy heavy resources in other modes 

        default:
            break;
    }
}
