#include "Controllers/UtilityController.h"

/*
Constructor
*/
UtilityController::UtilityController(
    ITerminalView& terminalView,
    IDeviceView& deviceView,
    IInput& terminalInput,
    IUtilityService& utilityService,
    IPinService& pinService,
    II2sService& i2sService,
    UserInputManager& userInputManager,
    PinAnalyzer& pinAnalyzer,
    AliasManager& aliasManager,
    ArgTransformer& argTransformer,
    TerminalCommandTransformer& commandTransformer,
    IShell& sysInfoShell,
    IShell& guideShell,
    HelpShell& helpShell,
    IShell& profileShell
)
    : terminalView(terminalView),
      deviceView(deviceView),
      terminalInput(terminalInput),
      utilityService(utilityService),
      pinService(pinService),
      i2sService(i2sService),
      userInputManager(userInputManager),
      pinAnalyzer(pinAnalyzer),
      aliasManager(aliasManager),
      commandTransformer(commandTransformer),
      argTransformer(argTransformer),
      sysInfoShell(sysInfoShell),
      guideShell(guideShell),
      helpShell(helpShell),
      profileShell(profileShell)
{}

/*
Entry point for command
*/
void UtilityController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "help" || cmd.getRoot() == "h" || cmd.getRoot() == "?") handleHelp();
    else if (cmd.getRoot() == "P")                                               handleEnablePullups();
    else if (cmd.getRoot() == "p")                                               handleDisablePullups();
    else if (cmd.getRoot() == "logic")                                           handleLogicAnalyzer(cmd);
    else if (cmd.getRoot() == "analogic")                                        handleAnalogic(cmd);
    else if (cmd.getRoot() == "system" || cmd.getRoot() == "sys")                handleSystem();
    else if (cmd.getRoot() == "guide")                                           handleGuide();
    else if (cmd.getRoot() == "man")                                             handleGuide();
    else if (cmd.getRoot() == "wizard")                                          handleWizard(cmd);
    else if (cmd.getRoot() == "hex" || cmd.getRoot() == "dec")                   handleHex(cmd);
    else if (cmd.getRoot() == "profile")                                         handleProfile();
    else if (cmd.getRoot() == "listen")                                          handleListen(cmd);
    else if (cmd.getRoot() == "delay" || cmd.getRoot() == "delayms")             handleDelay(cmd);
    else if (cmd.getRoot() == "delayus")                                         handleDelay(cmd);
    else if (cmd.getRoot() == "alias")                                           handleAlias();
    else {
        // just display commands for the mode without prompting
        helpShell.run(state.getCurrentMode(), false);
    }
}

/*
Mode Change
*/
ModeEnum UtilityController::handleModeChangeCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() != "mode" && cmd.getRoot() != "m") {
        terminalView.println("Invalid command for mode change.");
        return ModeEnum::None;
    }

    if (!cmd.getSubcommand().empty()) {
        ModeEnum newMode = ModeEnumMapper::fromString(cmd.getSubcommand());
        if (newMode != ModeEnum::None) {
            terminalView.println("Mode changed to " + ModeEnumMapper::toString(newMode));
            terminalView.println(""); 
            return newMode;
        } else {
            terminalView.println("Unknown mode: " + cmd.getSubcommand());
            return ModeEnum::None;
        }
    }

    return handleModeSelect();
}

/*
Mode Select
*/
ModeEnum UtilityController::handleModeSelect() {
    terminalView.println("");
    terminalView.println("Select mode:");
    std::vector<ModeEnum> modes;

    for (int i = 0; i < static_cast<int>(ModeEnum::COUNT); ++i) {
        ModeEnum mode = static_cast<ModeEnum>(i);
        std::string name = ModeEnumMapper::toString(mode);
        if (!name.empty()) {
            modes.push_back(mode);
            terminalView.println("  " + std::to_string(modes.size()) + ". " + name);
        }
    }

    terminalView.println("");
    terminalView.print("Mode Number > ");
    auto modeNumber = userInputManager.readModeNumber();

    if (modeNumber == 0xFF) {
        terminalView.println("");
        terminalView.println("");
        terminalView.println("Invalid input.");
        return ModeEnum::None;
    } else if (modeNumber >= 1 && modeNumber <= modes.size()) {
        ModeEnum selected = modes[modeNumber - 1];
        if (static_cast<int>(selected) > 9) {
            terminalView.println(""); // Hack to render correctly on web terminal
        }
        terminalView.println("");
        terminalView.println("Mode changed to " + ModeEnumMapper::toString(selected));
        terminalView.println("");
        return selected;
    } else {
        terminalView.println("");
        terminalView.println("Invalid mode number.");
        terminalView.println("");
        return ModeEnum::None;
    }
}

/*
Pullup: p
*/
void UtilityController::handleDisablePullups() {
    auto mode = state.getCurrentMode();
    switch (mode) {
        case ModeEnum::SPI:
            pinService.setInput(state.getSpiMISOPin());
            terminalView.println("SPI: Pull-ups disabled on MISO");
            break;

        case ModeEnum::I2C:
            pinService.setInput(state.getI2cSdaPin());
            pinService.setInput(state.getI2cSclPin());
            terminalView.println("I2C: Pull-ups disabled on SDA, SCL.");
            break;

        case ModeEnum::OneWire:
            pinService.setInput(state.getOneWirePin());
            terminalView.println("1-Wire: Pull-up disabled on DQ.");
            break;

        case ModeEnum::UART:
            pinService.setInput(state.getUartRxPin());
            terminalView.println("UART: Pull-ups disabled on RX.");
            break;

        case ModeEnum::HDUART:
            pinService.setInput(state.getHdUartPin());
            terminalView.println("HDUART: Pull-up disabled on IO pin.");
            break;

        case ModeEnum::TwoWire:
            pinService.setInput(state.getTwoWireIoPin());
            terminalView.println("2-WIRE: Pull-up disabled on DATA pin.");
            break;

        case ModeEnum::JTAG:
            for (auto pin : state.getJtagScanPins()) {
                pinService.setInput(pin);
            }
            terminalView.println("JTAG: Pull-ups disabled on all scan pins.");
            break;

        default:
            terminalView.println("Pull-ups not applicable for this mode.");
            break;
    }
}

/*
Pullup P
*/
void UtilityController::handleEnablePullups() {
    auto mode = state.getCurrentMode();
    switch (mode) {
        case ModeEnum::SPI:
            pinService.setInput(state.getSpiMISOPin());
            pinService.setInputPullup(state.getSpiMISOPin());
            terminalView.println("SPI: Pull-up enabled on MISO.");
            break;

        case ModeEnum::I2C:
            pinService.setInputPullup(state.getI2cSdaPin());
            pinService.setInputPullup(state.getI2cSclPin());
            terminalView.println("I2C: Pull-ups enabled on SDA, SCL.");
            break;

        case ModeEnum::OneWire:
            pinService.setInputPullup(state.getOneWirePin());
            terminalView.println("1-Wire: Pull-up enabled on DQ.");
            break;

        case ModeEnum::UART:
            pinService.setInputPullup(state.getUartRxPin());
            terminalView.println("UART: Pull-up enabled on RX.");
            break;

        case ModeEnum::HDUART:
            pinService.setInputPullup(state.getHdUartPin());
            terminalView.println("HDUART: Pull-up enabled on IO pin.");
            break;

        case ModeEnum::TwoWire:
            pinService.setInputPullup(state.getTwoWireIoPin());
            terminalView.println("2-WIRE: Pull-up enabled on DATA pin.");
            break;

        case ModeEnum::JTAG:
            for (auto pin : state.getJtagScanPins()) {
                pinService.setInputPullup(pin);
            }
            terminalView.println("JTAG: Pull-ups enabled on all scan pins.");
            break;

        default:
            terminalView.println("Pull-ups not applicable for this mode.");
            break;
    }
}

/*
Logic
*/
void UtilityController::handleLogicAnalyzer(const TerminalCommand& cmd) {

    uint16_t tDelay = 500;
    uint16_t inc = 100; // tDelay will be decremented/incremented by inc
    uint8_t step = 1; // step of the trace display kind of a zoom

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: logic <gpio>");
        return;
    }

    // Verify protected pin
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    /*
    auto forbidden = state.getProtectedPins();
    if (std::find(forbidden.begin(), forbidden.end(), pin) != forbidden.end()) {
        terminalView.println("Logic Analyzer: This pin is protected or reserved.");
        return;
    }
*/
    if (state.isPinProtected(pin)) {
        terminalView.println("Logic Analyzer: This GPIO is protected or reserved.");
        return;
    }
    terminalView.println("\nLogic Analyzer: Monitoring GPIO " + std::to_string(pin) + "... Press [ENTER] to stop.");
    terminalView.println("Displaying waveform on the ESP32 screen...\n");


    pinService.setInput(pin);
    std::vector<uint8_t> buffer;
    buffer.reserve(320); // 320 samples

    uint32_t lastCheck = utilityService.nowMs();
    deviceView.clear();
    deviceView.topBar("Logic Analyzer", false, false);

    while (true) {
        // Enter press
        if (utilityService.nowMs() - lastCheck > 10) {
            lastCheck = utilityService.nowMs();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                // fdufnews 2025/10/24 added to restore cursor position when leaving
                if (state.getTerminalMode() == TerminalTypeEnum::SerialPort)
                    terminalView.print("\n\n\n\n\r"); // 4 lines down to place cursor just under the logic trace
                terminalView.println("Logic Analyzer: Stopped by user.");
                break;
            }
            if (c == 's'){
                if (tDelay > inc){
                    tDelay -= inc;
                    terminalView.println("delay : " + std::to_string(tDelay) + "\n");
                }
            };
            if (c == 'S'){
                if (tDelay < 10000){
                    tDelay += inc;
                    terminalView.println("delay : " + std::to_string(tDelay) + "\n");
                }
            };
            if (c == 'z'){
                if (step > 1){
                    step--;
                    terminalView.println("step : " + std::to_string(step) + "\n");
                }
            };
            if (c == 'Z'){
                if (step < 4){
                    step++;
                    terminalView.println("step : " + std::to_string(step) + "\n");
                }
            };
        }

        // Draw
        if (buffer.size() >= 320) {
            buffer.resize(320);
            deviceView.drawLogicTrace(pin, buffer, step);

            // The poor man's drawLogicTrace() on terminal
            // draws a 132 samples sub part of the buffer to speed up the things
            if (state.getTerminalMode() == TerminalTypeEnum::SerialPort){
                terminalView.println("");
                uint8_t pos = 0;
                for(size_t i = 0; i < 132; i++, pos++){
                     terminalView.print(buffer[i]?"-":"_");
                }
                terminalView.print("\r\x1b[A");  // Up 1 line to put cursor at the correct place for the next draw
            }
            buffer.clear();
        }

        buffer.push_back(pinService.read(pin));
        utilityService.sleepUs(tDelay);
    }
}

/*
Analogic
*/
void UtilityController::handleAnalogic(const TerminalCommand& cmd) {
    uint16_t tDelay = 500;
    uint16_t inc = 100; // tDelay will be decremented/incremented by inc
    uint8_t step = 1; // step of the trace display kind of a zoom

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: analogic <gpio>");
        return;
    }

    // Verify protected pin
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (state.isPinProtected(pin)) {
        terminalView.println("Analogic: This GPIO is protected or reserved.");
        return;
    }
    if (!state.isPinAnalog(pin)){
        terminalView.println("Analogic: This GPIO is not an analog one");
        return;
    };

    terminalView.println("\nAnalogic: Monitoring GPIO " + std::to_string(pin) + "... Press [ENTER] to stop.");
    terminalView.println("Displaying waveform on the ESP32 screen...\n");


    pinService.setInput(pin);
    std::vector<uint8_t> buffer;
    buffer.reserve(320); // 320 samples

    uint32_t lastCheck = utilityService.nowMs();
    deviceView.clear();
    deviceView.topBar("Analog plotter", false, false);
    int count = 0;
    while (true) {
        // Enter press
        if (utilityService.nowMs() - lastCheck > 10) {
            lastCheck = utilityService.nowMs();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\nAnalogic: Stopped by user.");
                break;
            }
            if (c == 's'){
                if (tDelay > inc){
                    tDelay -= inc;
                    terminalView.println("\ndelay : " + std::to_string(tDelay) + "\n");
                }
            };
            if (c == 'S'){
                if (tDelay < 10000){
                    tDelay += inc;
                    terminalView.println("\ndelay : " + std::to_string(tDelay) + "\n");
                }
            };
            if (c == 'z'){
                if (step > 1){
                    step--;
                    terminalView.println("\nstep : " + std::to_string(step) + "\n");
                }
            };
            if (c == 'Z'){
                if (step < 4){
                    step++;
                    terminalView.println("\nstep : " + std::to_string(step) + "\n");
                }
            };
            count++;
            if ((count > 50) && (state.getTerminalMode() != TerminalTypeEnum::Standalone)){
                int raw = pinService.readAnalog(pin);
                float voltage = (raw / 4095.0f) * 3.3f;

                std::ostringstream oss;
                oss << "   Analog GPIO " << static_cast<int>(pin)
                    << ": " << raw
                    << " (" << voltage << " V)";
                terminalView.println(oss.str());
                count = 0;
            }
        }

        // Draw
        if (buffer.size() >= 320) {
            buffer.resize(320);
            deviceView.drawAnalogicTrace(pin, buffer, step);
            buffer.clear();
        }

        buffer.push_back(pinService.readAnalog(pin) >> 4); // convert the readAnalog() value to a uint8_t (4096 ==> 256)
        utilityService.sleepUs(tDelay);
    }
}

/*
System Information
*/
void UtilityController::handleSystem() {
    sysInfoShell.run();
}

/*
Firmware Guide (man)
*/
void UtilityController::handleGuide() {
    guideShell.run();
}

/*
Wizard
*/
void UtilityController::handleWizard(const TerminalCommand& cmd) {
    // Validate pin argument
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: wizard <gpio>");
        return;
    }

    // Verify protected pin
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (state.isPinProtected(pin)) {
        terminalView.println("Wizard: This GPIO is protected or reserved.");
        return;
    }

    terminalView.println("\nWizard: Please wait, analyzing GPIO " + std::to_string(pin) + "... Press [ENTER] to stop.\n");
    pinAnalyzer.begin(pin);
    const bool doPullTest = false; // TODO: add argument to enable pull test if needed

    while (true) {
        // Check for ENTER press to stop
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\nWizard: Stopped by user.");
            break;
        }

        // Sample pin
        for (int i = 0; i < 2048; i++) {
            pinAnalyzer.sample();
        }

        // Check if it's time to report and report activity
        if (pinAnalyzer.shouldReport(utilityService.nowMs())) {
            auto report = pinAnalyzer.buildReport(doPullTest);
            terminalView.print(pinAnalyzer.formatWizardReport(pin, report));
            pinAnalyzer.resetWindow();
            terminalView.println("Wizard: Analyzing GPIO " + std::to_string(pin) + "... Press [ENTER] to stop.\n");
        }
    }

    // Cleanup buffers
    pinAnalyzer.end();
}

/*
Listen
*/
void UtilityController::handleListen(const TerminalCommand& cmd) {
    // Pin arg
    uint8_t pin = 0;
    if (!cmd.getSubcommand().empty() && argTransformer.isValidNumber(cmd.getSubcommand())) {
        pin = argTransformer.toUint8(cmd.getSubcommand());
    } else {
        terminalView.println("Usage: listen <gpio>");
        return;
    }

    // Protected
    if (state.isPinProtected(pin)) {
        terminalView.println("Listen: This GPIO is protected or reserved.");
        return;
    }

    // Used by I2S
    if (state.getI2sBclkPin() == pin || state.getI2sLrckPin() == pin || state.getI2sDataPin() == pin) {
        terminalView.println("Listen: This GPIO is used by I2S.");
        return;
    }

    // Apply existing pull config
    IPinService::pullType pull = pinService.getPullType(pin);
    if (pull == IPinService::PULL_UP)         pinService.setInputPullup(pin);
    else if (pull == IPinService::PULL_DOWN)  pinService.setInputPullDown(pin);
    else                                     pinService.setInput(pin);

    // I2S init with configured pins
    i2sService.configureOutput(
        state.getI2sBclkPin(), state.getI2sLrckPin(), state.getI2sDataPin(),
        state.getI2sSampleRate(), state.getI2sBitsPerSample(),
        state.getI2sPercentLevel()
    );

    terminalView.println("\nListen: Activity to Audio @ GPIO " + std::to_string(pin) +
                         "... Press [ENTER] to stop.\n");

    terminalView.println(" [ℹ️  INFORMATION] ");
    terminalView.println(" Using I2S configured GPIOs for audio output.");
    terminalView.println(" You can set volume in the I2S mode settings.\n");

    //  params
    const uint16_t toneMs = 1;
    const uint16_t refreshUs = 200;
    const uint32_t windowUs = 20000;     // 20 ms
    const uint16_t fMin = 200;           // audio low
    const uint16_t fMax = 12000;         // audio high

    int last = pinService.read(pin);
    uint32_t windowStartUs = utilityService.nowUs();
    uint32_t edgesWindow = 0;

    while (true) {

        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // sample a bunch
        for (uint16_t i = 0; i < 256; i++) {
            int cur = pinService.read(pin);
            if (cur != last) {
                last = cur;
                edgesWindow++;
            }
        }

        uint32_t nowUs = utilityService.nowUs();
        if (nowUs - windowStartUs >= windowUs) {

            // approx edges per second
            float secs = (nowUs - windowStartUs) / 1000000.0f;
            float edgesPerSec = (secs > 0) ? (edgesWindow / secs) : 0.0f;
            float approxSignalHz = edgesPerSec * 0.5f;

            // clamp to something reasonable
            if (approxSignalHz < 0) approxSignalHz = 0;
            if (approxSignalHz > 50000) approxSignalHz = 50000;

            // Frequency has an impact on the sound
            float norm = approxSignalHz / 50000.0f;
            if (norm > 1) norm = 1;
            
            // audio freq to play based on activity
            uint16_t audioHz = fMin + (uint16_t)(norm * (float)(fMax - fMin));

            // Play tone only if we saw activity
            if (edgesWindow > 0) {
                i2sService.playTone(state.getI2sSampleRate(), audioHz, toneMs);
            }

            // reset window
            edgesWindow = 0;
            windowStartUs = nowUs;
        }

        utilityService.sleepUs(refreshUs);
    }

    terminalView.println("Listen: Stopped by user.\n");
}

/*
Hex
*/
void UtilityController::handleHex(const TerminalCommand& cmd) {
    uint32_t value = 0;

    // Parse number if any
    if (!cmd.getSubcommand().empty()) {
        const std::string& s = cmd.getSubcommand();

        if (!argTransformer.isValidNumber(s)) {
            terminalView.println("Usage: hex <number>");
            return;
        }

        value = argTransformer.parseHexOrDec32(s);
    } else {
        // No number provided, ask user for input
        value = (uint32_t)userInputManager.readValidatedUint32("\nEnter a number or hex", 65);
    }

    // dec
    terminalView.println("\n  Dec   : " + std::to_string(value));

    // hex
    terminalView.println("  Hex   : 0x" + argTransformer.toHex(value, 0));

    // bin 
    terminalView.println("  Bin   : " + argTransformer.toBinString(value));

    // ASCII
    std::string ascii = argTransformer.toAsciiString(value);
    if (!ascii.empty()) {
        terminalView.println("  ASCII : " + ascii);
    }

    terminalView.println("");
}

/*
Delay (cmd pipeline)
*/
void UtilityController::handleDelay(const TerminalCommand& cmd) {

    std::string s = cmd.getSubcommand();
    if (s.empty() || !argTransformer.isValidNumber(s)) {
        terminalView.println("Usage: delay <seconds>");
        terminalView.println("       delayms <milliseconds>");
        terminalView.println("       delayus <microseconds>");
        return;
    }

    uint32_t value = argTransformer.parseHexOrDec32(s);
    if (value == 0) return;

    std::string root = cmd.getRoot();

    // Convert to microseconds
    uint64_t us = value;
    if (root == "delay") {
        us = (uint64_t)value * 1000000ULL;
    } else if (root == "delayms") {
        us = (uint64_t)value * 1000ULL;
    } // delayus -> already us

    // Fast path
    if (us < 20000ULL) {
        utilityService.sleepUs(static_cast<uint32_t>(us));
        return;
    }

    // Long 
    uint64_t remaining = us;
    terminalView.println("\nDelaying for " + std::to_string(us / 1000) + " ms... Press [ENTER] to stop.\n");
    while (remaining > 0) {

        // Stop on ENTER 
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        uint32_t chunk = (remaining > 10000ULL) ? 10000U : (uint32_t)remaining;
        utilityService.sleepUs(chunk);
        remaining -= chunk;
    }
}

/*
Alias
*/
void UtilityController::handleAlias() {
    terminalView.println("");
    terminalView.println("=== Alias ===");
    terminalView.println("Create custom shortcuts.");
    terminalView.println("Simplify frequent commands.");
    terminalView.println("Group multiple commands to one.\n");

    // Command to alias
    std::string cmdLine = userInputManager.readString("Command to alias", "");
    if (cmdLine.empty()) {
        terminalView.println("Alias: Cancelled, empty command.\n");
        return;
    }

    // Name of the alias
    std::string aliasName = userInputManager.readString("Alias name (single word)", "");
    if (aliasName.empty()) {
        terminalView.println("Alias: Cancelled, empty name.\n");
        return;
    }

    // Validate alias name
    if (commandTransformer.isBuiltinCommand(aliasName)) {
        terminalView.println("\nAlias: Failed, name cannot be a built-in command.\n");
        return;
    }

    // Store the alias
    bool ok = aliasManager.add(aliasName, cmdLine);
    if (!ok) {
        size_t limit = aliasManager.sizeMax();
        terminalView.println("\nAlias: Failed, limited to " + std::to_string(limit) + " aliases.\n");
        return;
    }

    terminalView.println("\nAlias: Set, type '" + aliasName + "' to execute it.\n");
}

/*
Help
*/
void UtilityController::handleHelp() {
    // In HiZ, let the user pick a mode, in an active mode, jump straight to its help
    bool select = state.getCurrentMode() == ModeEnum::HIZ;
    helpShell.run(state.getCurrentMode(), select);
}

/*
Profile
*/
void UtilityController::handleProfile() {
    profileShell.run();
}
