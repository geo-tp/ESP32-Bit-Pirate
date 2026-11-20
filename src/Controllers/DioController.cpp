#include "Controllers/DioController.h"
#include <sstream>
#include <algorithm>
#include <map>

/*
Constructor
*/
DioController::DioController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IDeviceView& deviceView,
    IUtilityService& utilityService,
    IPinService& pinService,
    ArgTransformer& argTransformer,
    HelpShell& helpShell,
    UserInputManager& userInputManager
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      deviceView(deviceView),
      utilityService(utilityService),
      pinService(pinService),
      argTransformer(argTransformer),
      helpShell(helpShell),
      userInputManager(userInputManager) {}

/*
Entry point to handle a DIO command
*/
void DioController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "sniff" || cmd.getRoot() == "monitor") handleSniff(cmd);
    else if (cmd.getRoot() == "read")   handleReadPin(cmd); 
    else if (cmd.getRoot() == "set")    handleSetPin(cmd);
    else if (cmd.getRoot() == "scan")   handleScan(cmd);
    else if (cmd.getRoot() == "pins" || cmd.getRoot() == "status")   handlePins(cmd);
    else if (cmd.getRoot() == "pullup") handlePullup(cmd);
    else if (cmd.getRoot() == "pulldown") handlePulldown(cmd);
    else if (cmd.getRoot() == "pwm")    handlePwm(cmd);
    else if (cmd.getRoot() == "toggle") handleTogglePin(cmd);
    else if (cmd.getRoot() == "pulse")  handlePulse(cmd);
    else if (cmd.getRoot() == "measure") handleMeasure(cmd);
    else if (cmd.getRoot() == "servo")  handleServo(cmd);
    else if (cmd.getRoot() == "jam")    handleJamPin(cmd);
    else if (cmd.getRoot() == "reset")  handleResetPin(cmd);
    else                                handleHelp();
    
}

/*
Release (mode exit): stop active PWM and return every output-driven pin
to INPUT (high-Z, no pull). Silent, like I2cController::ensureReleased.
*/
void DioController::release() {
    pinService.releaseActivePwmPins();
    pinService.releaseOutputPins();
}

/*
Read
*/
void DioController::handleReadPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: read <gpio>");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Read")) return;
    int value = pinService.read(pin);
    terminalView.println("GPIO " + std::to_string(pin) + " = " + std::to_string(value) + (value ? " (HIGH)" : " (LOW)"));
}

/*
Set
*/
void DioController::handleSetPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: set <gpio> <IN/OUT/HI/LOW>");
        return;
    }

    std::string arg = cmd.getArgs();
    if (arg.empty()) {
        terminalView.println("DIO Set: Missing mode (IN/OUT/HI/LOW).");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Set")) return;
    char c = std::toupper(arg[0]);

    switch (c) {
        case 'I':
            pinService.setInput(pin);
            terminalView.println("DIO Set: GPIO " + std::to_string(pin) + " set to INPUT");
            break;
        case 'O':
            pinService.setOutput(pin);
            terminalView.println("DIO Set: GPIO " + std::to_string(pin) + " set to OUTPUT");
            break;
        case 'H':
        case '1':
            pinService.setOutput(pin);
            pinService.setHigh(pin);
            terminalView.println("DIO Set: Pin " + std::to_string(pin) + " set to HIGH");
            break;
        case 'L':
        case '0':
            pinService.setOutput(pin);
            pinService.setLow(pin);
            terminalView.println("DIO Set: Pin " + std::to_string(pin) + " set to LOW");
            break;
        default:
            terminalView.println("Unknown command. Use I, O, H (1), or L (0).");
            break;
    }
}

/*
Scan
*/
void DioController::handleScan(const TerminalCommand& cmd) {

    // Default pins 
    std::vector<uint8_t> defaultPins = state.getJtagScanPins();

    // Protected
    const std::vector<uint8_t> protectedPins = state.getProtectedPins();

    // Select
    std::vector<uint8_t> pins = userInputManager.readValidatedPinGroup(
        "GPIOs to detect activity",
        defaultPins,
        protectedPins
    );

    // Limit
    if (pins.size() > 8) {
        terminalView.println("Too many GPIOs selected, limiting to first 8.");
        pins.resize(8);
    }


    // Validate
    std::vector<uint8_t> validPins;
    for (uint8_t pin : pins) {
        if (isPinAllowed(pin, "Scan")) {
            validPins.push_back(pin);
        }
    }

    if (validPins.empty()) {
        terminalView.println("DIO Scan: No valid GPIOs selected.");
        return;
    }

    state.setJtagScanPins(validPins); //save for later use 

    terminalView.println("\nDIO Scan: Detecting activity... Press [ENTER] to stop.\n");

    // Configure pins as INPUT with existing pull
    for (uint8_t pin : validPins) {
        auto pull = pinService.getPullType(pin);
        if (pull == IPinService::PULL_UP)        pinService.setInputPullup(pin);
        else if (pull == IPinService::PULL_DOWN) pinService.setInputPullDown(pin);
        else                                    pinService.setInput(pin);
    }

    // Init maps
    std::map<uint8_t, int> lastState;
    std::map<uint8_t, uint32_t> edgeCount;
    std::vector<std::string> activeLines; //for deviceView pinout
    PinoutConfig cfg;
    PinoutConfig lastCfg; //to avoid updating deviceView if no change

    // high/low sample counters
    std::map<uint8_t, uint32_t> highSamples;
    std::map<uint8_t, uint32_t> lowSamples;

    for (uint8_t pin : validPins) {
        int v = pinService.read(pin);
        lastState[pin] = v;
        edgeCount[pin] = 0;
        highSamples[pin] = (v ? 1 : 0);
        lowSamples[pin]  = (v ? 0 : 1);
    }

    uint32_t lastPrint = utilityService.nowMs();

    while (true) {

        // Stop if ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("DIO Scan: Stopped by user.");
            break;
        }

        // Many samples per loop to have better stats 
        for (int i = 0; i < 256; i++) {
            for (uint8_t pin : validPins) {
                int cur = pinService.read(pin);

                if (cur) highSamples[pin]++; else lowSamples[pin]++;

                int& last = lastState[pin];
                if (cur != last) {
                    last = cur;
                    edgeCount[pin]++;
                }
            }
        }

        // Report each 2 sec
        uint32_t now = utilityService.nowMs();
        if (now - lastPrint >= 2000) {
            lastPrint = now;

            terminalView.println("Active GPIOs:");

            // Find max digits for align
            size_t maxEdgeDigits = 1;
            bool any = false;
            for (auto& [pin, edges] : edgeCount) {
                if (edges == 0) continue;
                any = true;

                // count digits
                uint32_t x = edges;
                size_t d = 1;
                while (x >= 10) { x /= 10; d++; }
                if (d > maxEdgeDigits) maxEdgeDigits = d;
            }

            if (!any) {
                terminalView.println("  (none)");
                terminalView.println("");
            } else {
                for (uint8_t pin : validPins) {
                    uint32_t edges = edgeCount[pin];
                    if (edges == 0) continue;

                    uint32_t hi = highSamples[pin];
                    uint32_t lo = lowSamples[pin];
                    uint32_t total = hi + lo;

                    // Percent
                    uint32_t hiPct = 0;
                    uint32_t loPct = 0;
                    if (total != 0) {
                        hiPct = (hi * 100u + (total / 2u)) / total;
                        loPct = 100u - hiPct; // keep sum to 100
                    }

                    // padding for edges alignment
                    // ex: edges=1 vs edges=32000
                    size_t digits = 1;
                    {
                        uint32_t x = edges;
                        while (x >= 10) { x /= 10; digits++; }
                    }
                    size_t pad = (maxEdgeDigits > digits) ? (maxEdgeDigits - digits) : 0;
                    std::string padSpaces(pad, ' ');

                    // Pad GPIO
                    std::string pinStr = std::to_string(pin);
                    if (pin < 10) pinStr += " ";

                    terminalView.println(
                        "  GPIO " + pinStr +
                        " | edges=" + padSpaces + std::to_string(edges) +
                        " | HIGH=" + std::to_string(hiPct) + "% LOW=" + std::to_string(loPct) + "%"
                    );

                    if (activeLines.size() >= 4) continue; //avoid overflowing deviceView
                    activeLines.push_back("GPIO " + std::to_string(pin));
                }
                
                // Update device view
                if (!activeLines.empty()) {
                    cfg.setMode("SCAN");
                    cfg.setMappings(activeLines);

                    if (lastCfg.getMappings() != cfg.getMappings()) {
                        lastCfg = cfg;
                        deviceView.show(cfg);
                    }
                    activeLines.clear();
                }
                
                terminalView.println("");
            }

            // Reset window counters
            for (uint8_t pin : validPins) {
                edgeCount[pin] = 0;
                highSamples[pin] = 0;
                lowSamples[pin] = 0;
            }
        }
    }
}

void DioController::handlePins(const TerminalCommand& cmd) {

    const std::vector<uint8_t> protectedPins = state.getProtectedPins();

    // All GPIO 0 to 48 except protected
    std::vector<uint8_t> validPins;
    validPins.reserve(49);

    for (uint8_t pin = 0; pin <= 48; ++pin) {
        if (std::find(protectedPins.begin(), protectedPins.end(), pin) != protectedPins.end()) {
            continue;
        }
        validPins.push_back(pin);
    }

    if (validPins.empty()) {
        terminalView.println("\nDIO Pins: No usable GPIOs (all protected?).\n");
        return;
    }

    terminalView.println("\nDIO Pins: Reading state of available GPIOs...\n");
    terminalView.println("GPIO |  PULL  |    STATE");
    terminalView.println("-----+--------+-------------");

    for (uint8_t pin : validPins) {

        auto pull = pinService.getPullType(pin);

        // PULL 
        std::string pullStr;
        if (pull == IPinService::PULL_UP)        pullStr = "PULLUP";
        else if (pull == IPinService::PULL_DOWN) pullStr = "PDOWN ";
        else                                    pullStr = "NOPULL";

        // STATE 
        bool bInput = pinService.isInputMode(pin);
        int v = 0;
        if (bInput) { // read return always 0 in output mode
            // https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html#_CPPv414gpio_get_level10gpio_num_t
            v = pinService.read(pin);
        }
        std::string stateStr = bInput ? "INPUT (" + std::string(v ? "HIGH" : "LOW") + ")" : "OUTPUT";

        // GPIO padded
        std::string gpioStr = std::to_string(pin);
        if (gpioStr.length() == 1) gpioStr = "  " + gpioStr;
        else if (gpioStr.length() == 2) gpioStr = " " + gpioStr;

        terminalView.println(" " + gpioStr + " | " + pullStr + " | " + stateStr);
    }

    terminalView.println("");
}

/*
Pullup
*/
void DioController::handlePullup(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: pullup <gpio>");
        return;
    }

    int pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pullup")) return;
    pinService.setInputPullup(pin);

    terminalView.println("DIO Pullup: Set on GPIO " + std::to_string(pin));
}

/*
Pulldown
*/
void DioController::handlePulldown(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: pulldown <gpio>");
        return;
    }

    int pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pulldown")) return;
    pinService.setInputPullDown(pin);

    terminalView.println("DIO Pulldown: Set on GPIO " + std::to_string(pin));
}

/*
Sniff
*/
void DioController::handleSniff(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: sniff <gpio>");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Sniff")) return;

    // Apply existing pull config
    IPinService::pullType pull = pinService.getPullType(pin);
    if (pull == IPinService::PULL_UP)       pinService.setInputPullup(pin);
    else if (pull == IPinService::PULL_DOWN) pinService.setInputPullDown(pin);
    else                                   pinService.setInput(pin);

    terminalView.println("DIO Sniff: GPIO " + std::to_string(pin) + "... Press [ENTER] to stop");
    uint8_t last = pinService.read(pin);
    terminalView.println("\nInitial state: " + std::string(last ? "HIGH" : "LOW"));
    terminalView.println("");

    uint32_t lastEdgeUs = utilityService.nowUs();
    uint32_t lastKeyPollMs = utilityService.nowMs();

    while (true) {
        // check ENTER every 10ms
        if (utilityService.nowMs() - lastKeyPollMs > 10) {
            lastKeyPollMs = utilityService.nowMs();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\nDIO Sniff: Stopped.");
                break;
            }
        }

        int current = pinService.read(pin);
        if (current != last) {
            uint32_t nowUs = utilityService.nowUs();
            uint32_t dtUs = nowUs - lastEdgeUs;
            lastEdgeUs = nowUs;

            std::string transition = (last == 0 && current == 1)
                ? "LOW  -> HIGH"
                : "HIGH -> LOW ";

            terminalView.println(
                "GPIO " + std::to_string(pin) + ": " + transition +
                " | delta=" + std::to_string(dtUs) + "us"
            );

            last = current;
        }
    }
}

/*
Pwm
*/
void DioController::handlePwm(const TerminalCommand& cmd) {
    auto sub = cmd.getSubcommand();
    auto args = argTransformer.splitArgs(cmd.getArgs());

    // Validate GPIO
    if (sub.empty() || !argTransformer.isValidNumber(sub)) {
        terminalView.println("Usage: pwm <gpio> [frequency] [duty]");
        return;
    }

    uint8_t pin = argTransformer.toUint8(sub);
    if (!isPinAllowed(pin, "PWM")) return;

    // Defaults
    uint32_t freq = 1000;
    uint8_t duty = 50;

    // freq / duty  ?
    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        freq = argTransformer.toUint32(args[0]);
    } else {
        // prompt freq 
        freq = userInputManager.readValidatedUint32("PWM frequency (Hz)", freq, false);
    }

    if (args.size() >= 2 && argTransformer.isValidNumber(args[1])) {
        duty = argTransformer.toUint8(args[1]);
    } else {
        // prompt duty
        duty = userInputManager.readValidatedUint8("PWM duty (%)", duty, 0, 100);
    }

    // Setup PWM
    bool ok = pinService.setupPwm(pin, freq, duty);
    if (!ok) {
        terminalView.println("DIO PWM: Cannot set " + std::to_string(pin) +
                            " either wrong argument or too many allocated PWM");
         return;
    }

    terminalView.println("\nDIO PWM: GPIO " + std::to_string(pin) +
                         " (" + std::to_string(freq) + "Hz, " +
                         std::to_string(duty) + "% duty)... Use 'reset " + 
                         std::to_string(pin) + "' to stop.\n");
}


/*
Measure edges
*/
void DioController::handleMeasure(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (cmd.getSubcommand().empty()) {
        terminalView.println("Usage: measure <gpio> [duration_ms]");
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("DIO Measure: Invalid GPIO number.");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Measure")) return;

    uint32_t durationMs = 1000;
    if (!args.empty() && argTransformer.isValidNumber(args[0])) {
        durationMs = std::min<uint32_t>(argTransformer.toUint32(args[0]), 5000UL);
        if (durationMs == 5000) {
            terminalView.println("Note: Duration limited to 5000 ms max.");
        }
    }

    terminalView.println("DIO Measure: Sampling GPIO " + std::to_string(pin) +
                         " for " + std::to_string(durationMs) + " ms...");

    IPinService::pullType pull =  pinService.getPullType(pin);
    
    switch (pull)
    {
    case IPinService::NOPULL:
        pinService.setInput(pin);
        break;
    case IPinService::PULL_UP:
        pinService.setInputPullup(pin);
        break;
    case IPinService::PULL_DOWN:
        pinService.setInputPullDown(pin);
        break;
    
    default:
        break;
    }
    
    int last = pinService.read(pin);
    uint32_t rising = 0, falling = 0;

    uint32_t startMs = utilityService.nowMs();

    while (utilityService.nowMs() - startMs < durationMs) {
        int current = pinService.read(pin);
        if (current != last) {
            if (last == 0 && current == 1) ++rising;
            else if (last == 1 && current == 0) ++falling;
            last = current;
        }
    }

    terminalView.println("");
    terminalView.println(" Results:");
    terminalView.println("  • Rising edges:     " + std::to_string(rising));
    terminalView.println("  • Falling edges:    " + std::to_string(falling));

    uint32_t totalEdges = rising + falling;
    float freqHz = (totalEdges / 2.0f) / (durationMs / 1000.0f);

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << "  • Approx. frequency: " << freqHz << " Hz\n";
    terminalView.println(oss.str());
}

/*
Toggle
*/
void DioController::handleTogglePin(const TerminalCommand& cmd) {
    
    auto args = argTransformer.splitArgs(cmd.getArgs());
    
    if (cmd.getSubcommand().empty() || args.empty()) {
        terminalView.println("Usage: toggle <gpio> <ms>");
        return;
    }
    
    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
    !argTransformer.isValidNumber(args[0])) {
        terminalView.println("DIO Toggle: Invalid arguments.");
        return;
    }
    
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Toggle")) return;

    uint32_t intervalMs = argTransformer.toUint32(args[0]);
    
    pinService.setOutput(pin);
    bool state = false;

    terminalView.println("\nDIO Toggle: GPIO " + std::to_string(pin) + " every " + std::to_string(intervalMs) + "ms...Press [ENTER] to stop.");
    terminalView.println("");

    uint32_t lastToggle = utilityService.nowMs();
    uint32_t lastCheck = utilityService.nowMs();

    while (true) {
        uint32_t now = utilityService.nowMs();

        // check ENTER press every 10ms
        if (now - lastCheck > 10) {
            lastCheck = now;
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("DIO Toggle: Stopped.");
                break;
            }
        }

        // toggle GPIO at defined interval
        if (now - lastToggle >= intervalMs) {
            lastToggle = now;
            state = !state;
            if (state) {
                pinService.setHigh(pin);
            } else {
                pinService.setLow(pin);
            }
        }
    }
}

/*
Reset
*/
void DioController::handleResetPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        terminalView.println("Usage: reset <gpio>");
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("DIO Reset: Invalid GPIO number.");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Reset")) return;

    pinService.detachSignal(pin); // detach PWM if active

    // Reset Pullup
    pinService.setInput(pin);

    terminalView.println("DIO Reset: GPIO " + std::to_string(pin) + " to INPUT (no pull-up, no PWM).");
}

void DioController::handleServo(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("Usage: servo <gpio> <angle>");
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidNumber(cmd.getArgs())) {
        terminalView.println("DIO Servo: Invalid arguments.");
        return;
    }

    uint8_t pin = argTransformer.parseHexOrDec(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Servo")) return;

    uint8_t angle = argTransformer.parseHexOrDec(cmd.getArgs());
    pinService.setServoAngle(pin, angle);
    terminalView.println("DIO Servo: Set GPIO " + std::to_string(pin) + 
                         " to angle " + std::to_string(angle) + 
                         "... Use 'reset " + std::to_string(pin) + "' to stop.");
}

/*
Pulse
*/
void DioController::handlePulse(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("Usage: pulse <gpio> <duration_us>");
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidNumber(cmd.getArgs())) {
        terminalView.println("DIO Pulse: Invalid arguments.");
        return;
    }

    uint8_t pin = argTransformer.parseHexOrDec(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pulse")) return;

    uint32_t durationUs = argTransformer.toUint32(cmd.getArgs());

    // Read current state to determine pulse level
    int base = pinService.read(pin);           // 0 or 1
    int pulseLevel = base ? 0 : 1;             // inverse

    // Set as output
    pinService.setOutput(pin);

    // Pulse inverse
    if (pulseLevel) pinService.setHigh(pin);
    else            pinService.setLow(pin);

    utilityService.sleepUs(durationUs);

    if (base) pinService.setHigh(pin);
    else      pinService.setLow(pin);

    terminalView.println(
        "DIO Pulse: GPIO " + std::to_string(pin) +
        " pulsed " + std::string(pulseLevel ? "HIGH" : "LOW") +
        " for " + std::to_string(durationUs) + " us."
    );
}

/*
Jam
*/
void DioController::handleJamPin(const TerminalCommand& cmd) {

    auto args = argTransformer.splitArgs(cmd.getArgs());
    uint32_t minUs = 5;
    uint32_t maxUs = 100;

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("Usage: jam <gpio> [min_us] [max_us]");
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Jam")) return;


    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        minUs = argTransformer.toUint32(args[0]);
    }
    if (args.size() >= 2 && argTransformer.isValidNumber(args[1])) {
        maxUs = argTransformer.toUint32(args[1]);
    }

    if (minUs < 1) minUs = 1;
    if (maxUs < minUs) maxUs = minUs;

    pinService.setOutput(pin);

    terminalView.println("DIO Jam: GPIO " + std::to_string(pin) +
                         " random toggle [" + std::to_string(minUs) + ".." + std::to_string(maxUs) +
                         "] us... Press [ENTER] to stop.");
    terminalView.println("");

    bool state = false;
    uint32_t lastCheck = utilityService.nowMs();

    while (true) {
        // check ENTER press every 10ms
        if (utilityService.nowMs() - lastCheck > 10) {
            lastCheck = utilityService.nowMs();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("DIO Jam: Stopped by user.");
                break;
            }
        }

        state = !state;
        if (state) pinService.setHigh(pin);
        else       pinService.setLow(pin);

        // random delay between minUs and maxUs
        uint32_t span = (maxUs - minUs);
        uint32_t waitUs = minUs + (span ? (utilityService.randomUint32() % (span + 1)) : 0);
        utilityService.sleepUs(waitUs);
    }
}

/*
Help
*/
void DioController::handleHelp() {
    terminalView.println("\nUnknown command. Available DIO commands:");
    helpShell.run(state.getCurrentMode(), false);
}

bool DioController::isPinAllowed(uint8_t pin, const std::string& context) {
    const auto& protectedPins = state.getProtectedPins();
    
    if (std::find(protectedPins.begin(), protectedPins.end(), pin) != protectedPins.end()) {
        terminalView.println("DIO " + context + ": GPIO " + std::to_string(pin) + " is protected and cannot be used.");
        return false;
    }

    if (pin > 48 /* max pin for S3 */) {
        terminalView.println("DIO " + context + ": GPIO " + std::to_string(pin) + " is out of range (0-48).");
        return false;
    }

    if (pin < 0) {
        terminalView.println("DIO " + context + ": GPIO " + std::to_string(pin) + " is invalid.");
        return false;
    }

    return true;
}

std::vector<std::string> DioController::buildPullConfigLines() {
    std::vector<std::string> lines;

    auto pins = pinService.getConfiguredPullPins();
    std::sort(pins.begin(), pins.end());

    size_t total = 0;

    for (uint8_t pin : pins) {
        auto pull = pinService.getPullType(pin);

        if (pull == IPinService::PULL_UP) {
            lines.push_back("GPIO " + std::to_string(pin) + " PULLUP");
        } 
        else if (pull == IPinService::PULL_DOWN) {
            lines.push_back("GPIO " + std::to_string(pin) + " PULLDOWN");
        } else {
            continue;
        }

        ++total;

        // Limit to 4 displayable lines
        if (lines.size() == 4) {
            if (pins.size() > total) {
                lines.back() += " ...";
            }
            break;
        }
    }

    return lines;
}
