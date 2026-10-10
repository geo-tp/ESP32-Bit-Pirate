#include "I2cController.h"
#include <cerrno>
#include <cstdlib>
#include <iomanip>

/*
Constructor
*/
I2cController::I2cController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IUtilityService& utilityService,
    II2cService& i2cService,
    II2cSnifferService& i2cSnifferService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    II2cEepromShell& eepromShell,
    HelpShell& helpShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      utilityService(utilityService),
      i2cService(i2cService),
      i2cSnifferService(i2cSnifferService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      eepromShell(eepromShell),
      helpShell(helpShell)
{}

/*
Entry point to handle I2C command
*/
void I2cController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "scan") handleScan();
    else if (cmd.getRoot() == "discovery") handleDiscover();
    else if (cmd.getRoot() == "sniff") handleSniff(cmd);
    else if (cmd.getRoot() == "freq") handleFrequency();
    else if (cmd.getRoot() == "ping") handlePing(cmd);
    else if (cmd.getRoot() == "identify") handleIdentify(cmd);
    else if (cmd.getRoot() == "write") handleWrite(cmd);
    else if (cmd.getRoot() == "read") handleRead(cmd);
    else if (cmd.getRoot() == "dump") handleDump(cmd);
    else if (cmd.getRoot() == "regs") handleRegs(cmd);
    else if (cmd.getRoot() == "slave") handleSlave(cmd);
    else if (cmd.getRoot() == "glitch") handleGlitch(cmd);
    else if (cmd.getRoot() == "flood") handleFlood(cmd);
    else if (cmd.getRoot() == "jam") handleJam();
    else if (cmd.getRoot() == "eeprom") handleEeprom(cmd);
    else if (cmd.getRoot() == "recover") handleRecover();
    else if (cmd.getRoot() == "monitor") handleMonitor(cmd);
    else if (cmd.getRoot() == "trace") handleTrace(cmd);
    else if (cmd.getRoot() == "swap") handleSwap();
    else if (cmd.getRoot() == "health") handleHealth(cmd);
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point to handle I2C instruction
*/
void I2cController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = i2cService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("I2C Read:\n");
        terminalView.println(result);
    }
}

/*
Scan
*/
void I2cController::handleScan() {
    terminalView.println("I2C Scan: Scanning I2C bus... Press [ENTER] to stop");
    terminalView.println("");
    bool found = false;

    for (uint8_t addr = 1; addr < 127; ++addr) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("I2C Scan: Cancelled by user.");
            return;
        }
        
        i2cService.beginTransmission(addr);
        if (i2cService.endTransmission() == 0) {
            std::stringstream ss;
            ss << "Found device at 0x" << std::hex << std::uppercase << (int)addr;
            terminalView.println(ss.str());
            found = true;
        }
    }

    if (!found) {
        terminalView.println("I2C Scan: No I2C devices found.");
    }
    terminalView.println("");
}

/*
Sniff
*/    
void I2cController::handleSniff(const TerminalCommand& cmd) {
    if (!cmd.getArgs().empty()) {
        terminalView.println("Usage: sniff [addr]");
        return;
    }

    bool filterEnabled = false;
    uint8_t filterAddress = 0;

    if (!cmd.getSubcommand().empty()) {
        if (!tryParseAddress(cmd.getSubcommand(), filterAddress)) {
            terminalView.println("I2C Sniffer: Invalid address. Use decimal or 0x-prefixed hex (0x00-0x7F).");
            return;
        }
        filterEnabled = true;
    }

    if (filterEnabled) {
        terminalView.println("I2C Sniffer: Listening for address 0x" +
                             argTransformer.toHex(filterAddress) +
                             " on SCL/SDA... Press [ENTER] to stop.\n");
    } else {
        terminalView.println("I2C Sniffer: Listening on SCL/SDA... Press [ENTER] to stop.\n");
    }

    i2cSnifferService.setAddressFilter(filterEnabled, filterAddress);
    i2cSnifferService.begin(state.getI2cSclPin(), state.getI2cSdaPin()); // dont need freq to work
    if (!i2cSnifferService.setup()) {
        i2cSnifferService.setAddressFilter(false, 0);
        i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());
        terminalView.println(std::string("I2C Sniffer: ") + i2cSnifferService.backendName() +
                             " initialization failed: " + i2cSnifferService.lastError());
        return;
    }

    // Lazy CLI output buffer: only reserve RAM after the first successful
    // sniff initialization, then reuse its capacity until I2C mode exit.
    if (sniffLine.capacity() < 256) sniffLine.reserve(256);
    sniffLine.clear();

    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        // Keep capture/output draining fast, but periodically return to the
        // input poll so ENTER can still stop a continuously busy I2C bus.
        size_t charsThisPass = 0;
        static constexpr size_t MAX_CHARS_PER_PASS = 512;
        while (charsThisPass < MAX_CHARS_PER_PASS && i2cSnifferService.available()) {
            char c = i2cSnifferService.read();
            charsThisPass++;

            if (c == '\n') {
                sniffLine += "  ";
                terminalView.println(sniffLine);
                sniffLine.clear();
            } else if (c != '\0') {
                sniffLine += c;
            }
        }
    }

    // Stop IRQ capture before clearing the rings so a late edge cannot refill
    // a buffer that is supposed to belong to the next sniff session.
    i2cSnifferService.stop();
    i2cSnifferService.resetBuffer();
    i2cSnifferService.setAddressFilter(false, 0);
    i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());
    terminalView.println("\n\nI2C Sniffer: Stopped.");
}

void I2cController::handleFrequency() {
    terminalView.println("I2C Freq: Listening on SCL... Press [ENTER] to stop.");

    terminalView.println("");
    terminalView.println(" [ℹ️  INFORMATION] ");
    terminalView.println(" Wait for I2C traffic from a master");
    terminalView.println(" to measure the SCL clock frequency.");
    terminalView.println("");

    i2cService.end();
    I2cFrequencyResult result;
    bool cancelled = false;
    const bool success = i2cSnifferService.measureFrequency(
        state.getI2cSclPin(), 0, result, [this, &cancelled]() {
            const char key = terminalInput.readChar();
            cancelled = key == '\r' || key == '\n';
            return cancelled;
        });
    i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());

    if (!success) {
        terminalView.println(std::string("Freq error: ") + i2cSnifferService.lastError());
        return;
    }
    if (cancelled) {
        terminalView.println("I2C Freq: Stopped by user.\n");
        return;
    }
    if (!result.reliable) {
        terminalView.println("I2C Freq: No stable clock.\n");
        return;
    }

    terminalView.println("\n=== Frequency Report ===");

    std::ostringstream report;
    report << std::fixed << std::setprecision(2)
           << "Freq: " << result.frequencyHz / 1000.0 << " kHz\n\r"
           << "Period: " << result.periodUs << " us\n\r"
           << "LOW: " << result.lowUs << " us\n\r"
           << "HIGH: " << result.highUs << " us\n\r"
           << "Cycles: " << result.acceptedCycles << "/" << result.capturedCycles;
    terminalView.println(report.str());
    terminalView.println("========================\n");

    auto confirm = userInputManager.readYesNo("Save frequency to the configuration?", false);
    if (confirm) {
        state.setI2cFrequency(result.frequencyHz);
        terminalView.println("Frequency saved to configuration.");
    }
    terminalView.println("");
}

/*
Ping
*/
void I2cController::handlePing(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        terminalView.println("Usage: ping <I2C address>");
        return;
    }

    const std::string& arg = cmd.getSubcommand();
    uint8_t address = 0;
    if (!tryParseAddress(arg, address)) {
        terminalView.println("I2C Ping: Invalid address. Use decimal or 0x-prefixed hex.");
        return;
    }

    std::stringstream result;
    result << "Ping 0x" << std::hex << std::uppercase << (int)address << ": ";

    i2cService.beginTransmission(address);
    uint8_t i2cResult = i2cService.endTransmission();

    if (i2cResult == 0) {
        result << "I2C Ping: ACK received! Device is present.";
    } else {
        result << "I2C Ping: No response (NACK or error).";
    }

    terminalView.println(result.str());
}

/*
Write
*/
void I2cController::handleWrite(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    // addr
    if (cmd.getSubcommand().empty()) {
        terminalView.println("Usage: write <addr> [reg] [val]");
        return;
    }

    const std::string& addrStr = cmd.getSubcommand();

    uint8_t addr = 0;
    if (!tryParseAddress(addrStr, addr)) {
        terminalView.println("Error: Invalid address. Use decimal or 0x-prefixed hex.");
        return;
    }

    // reg
    uint8_t reg = 0;
    if (args.size() >= 1) {
        if (!argTransformer.isValidNumber(args[0])) {
            terminalView.println("Error: Invalid register. Use decimal or 0x-prefixed hex.");
            return;
        }
        reg = argTransformer.parseHexOrDec(args[0]);
    } else {
        reg = (uint8_t)userInputManager.readValidatedByte("Register to write", 0, true);
    }

    // val
    uint8_t val = 0;
    if (args.size() >= 2) {
        if (!argTransformer.isValidNumber(args[1])) {
            terminalView.println("Error: Invalid value. Use decimal or 0x-prefixed hex.");
            return;
        }
        val = argTransformer.parseHexOrDec(args[1]);
    } else {
        val = (uint8_t)userInputManager.readValidatedByte("Value to write", 0, true);
    }

    // Ping addr
    i2cService.beginTransmission(addr);
    uint8_t pingResult = i2cService.endTransmission();
    if (pingResult != 0) {
        terminalView.println("I2C Ping: 0x" + argTransformer.toHex(addr) + " no response. Aborting write.");
        return;
    }

    if (!userInputManager.readYesNo(
            "Write 0x" + argTransformer.toHex(val) +
            " to reg 0x" + argTransformer.toHex(reg) +
            " @ dev 0x" + argTransformer.toHex(addr) + "?",
            true)) {
        terminalView.println("I2C Write: Cancelled.");
        return;
    }

    // Write
    i2cService.beginTransmission(addr);
    i2cService.write(reg);
    i2cService.write(val);
    i2cService.endTransmission(true);

    terminalView.println(
        "I2C Write: 0x" + argTransformer.toHex(val) +
        " -> reg 0x" + argTransformer.toHex(reg) +
        " @ dev 0x" + argTransformer.toHex(addr) + "."
    );
}

/*
Read
*/
void I2cController::handleRead(const TerminalCommand& cmd) {
    // addr 
    if (cmd.getSubcommand().empty()) {
        terminalView.println("Usage: read <addr> [reg]");
        return;
    }

    const std::string& addrStr = cmd.getSubcommand();
    uint8_t addr = 0;
    if (!tryParseAddress(addrStr, addr)) {
        terminalView.println("Error: Invalid address. Use decimal or 0x-prefixed hex.");
        return;
    }

    // reg
    uint8_t reg = 0;
    if (!cmd.getArgs().empty()) {
        if (!argTransformer.isValidNumber(cmd.getArgs())) {
            terminalView.println("Error: Invalid register. Use decimal or 0x-prefixed hex.");
            return;
        }
        reg = argTransformer.parseHexOrDec(cmd.getArgs());
    } else {
        reg = (uint8_t)userInputManager.readValidatedByte("Register to read", 0, true);
    }

    // Check I2C device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Read: No device found at 0x" + argTransformer.toHex(addr));
        return;
    }

    // Write register address first
    i2cService.beginTransmission(addr);
    i2cService.write(reg);
    i2cService.endTransmission(false);

    i2cService.requestFrom(addr, 1);
    if (i2cService.available()) {
        uint8_t value = (uint8_t)i2cService.read();
        terminalView.println(
            "I2C Read: 0x" + argTransformer.toHex(value) +
            " (" + std::to_string((int)value) + ")" +
            " from reg 0x" + argTransformer.toHex(reg) +
            " @ dev 0x" + argTransformer.toHex(addr) + "."
        );
    } else {
        terminalView.println("I2C Read: No data available.");
    }
}

/*
Config
*/
void I2cController::handleConfig() {
    terminalView.println("I2C Configuration:");

    auto forbidden = state.getProtectedPins();

    uint8_t sda = userInputManager.readValidatedPinNumber("SDA GPIO", state.getI2cSdaPin(), forbidden);
    state.setI2cSdaPin(sda);
    forbidden.push_back(sda);

    uint8_t scl = userInputManager.readValidatedPinNumber("SCL GPIO", state.getI2cSclPin(), forbidden);
    state.setI2cSclPin(scl);
    forbidden.push_back(scl);
    
    uint32_t freq = userInputManager.readValidatedUint32("Frequency", state.getI2cFrequency());
    state.setI2cFrequency(freq);

    i2cService.configure(sda, scl, freq);

    terminalView.println("I2C configured.\n");
}

/*
Slave
*/
void I2cController::handleSlave(const TerminalCommand& cmd) {
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: slave <addr>");
        return;
    }

    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();

    // Validate arg
    if (addr < 0x08 || addr > 0x77) {
        terminalView.println("I2C Slave: Invalid address. Must be between 0x08 and 0x77.");
        return;
    }

    terminalView.println("I2C Slave: Listening on address 0x" + argTransformer.toHex(addr) +
                         "... Press [ENTER] to stop.\n");
    
    // Start slave
    i2cService.clearSlaveLog();
    i2cService.beginSlave(addr, sda, scl);
    std::vector<std::string> currentLog;
    currentLog.reserve(II2cService::SLAVE_LOG_MAX);
    uint32_t lastLogCount = 0;

    std::vector<std::string> lastLog;
    while (true) {
        // Enter press
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        // Get master log from slave and display it
        currentLog = i2cService.getSlaveLog();
        uint32_t count = i2cService.getSlaveLogCount();
        if (count != lastLogCount) {
            currentLog = i2cService.getSlaveLog();

            uint32_t delta = count - lastLogCount;
            if (delta > currentLog.size()) delta = currentLog.size(); // cap

            // DIsplay new entries
            size_t start = currentLog.size() - delta;
            for (size_t i = start; i < currentLog.size(); ++i) {
                terminalView.println(currentLog[i]);
            }

            lastLogCount = count;
        }
    }

    // Close slave
    i2cService.endSlave();
    i2cService.clearSlaveLog();
    ensureConfigured();
    terminalView.println("\nI2C Slave: Stopped by user.");
}

/*
Dump
*/
void I2cController::handleDump(const TerminalCommand& cmd) {
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: dump <addr> [length]");
        return;
    }
    uint16_t start = 0x00;
    uint16_t len = 256;

    // Presence check 
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission(true) != 0) { 
        terminalView.println("I2C Dump: No device found at " + cmd.getSubcommand());
        return;
    }

    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        len = argTransformer.parseHexOrDec16(args[0]);
    }
    if (len == 0) len = 1;
    if (len > 256) len = 256;

    std::vector<uint8_t> values(len, 0xFF);
    std::vector<bool> valid(len, false);

    terminalView.println("I2C Dump: 0x" + argTransformer.toHex(addr) +
                         " from 0x" + argTransformer.toHex(start) +
                         " for " + std::to_string(len) + " bytes... Press [ENTER] to stop.\n");

    // Try register style
    performRegisterRead(addr, start, len, values, valid);

    bool any = std::any_of(valid.begin(), valid.end(), [](bool b){ return b; });

    // fallback raw if nothing read
    if (!any) {
        terminalView.println("\nI2C Dump: register read failed — trying raw read...");
        performRawRead(addr, start, len, values, valid);
        any = std::any_of(valid.begin(), valid.end(), [](bool b){ return b; });
    }

    if (!any) {
        terminalView.println("I2C Dump: Unable to read any data — device NACKed or unsupported protocol.\n");
        return;
    }

    printHexDump(start, len, values, valid);
}

void I2cController::performRegisterRead(uint8_t addr, uint16_t start, uint16_t len,
                                        std::vector<uint8_t>& values, std::vector<bool>& valid) {
    const uint8_t CHUNK_SIZE = 16;
    const bool use16bitAddr = (start + len - 1) > 0xFF;

    for (uint16_t offset = 0; offset < len; offset += CHUNK_SIZE) {
        uint16_t reg = start + offset;
        uint8_t toRead = (offset + CHUNK_SIZE <= len) ? CHUNK_SIZE : (uint8_t)(len - offset);

        // set register pointer
        i2cService.beginTransmission(addr);
        if (use16bitAddr) {
            i2cService.write((uint8_t)((reg >> 8) & 0xFF));
            i2cService.write((uint8_t)(reg & 0xFF));
        } else {
            i2cService.write((uint8_t)(reg & 0xFF));
        }
        if (i2cService.endTransmission(false) != 0) {
            // NACK on pointer write
            continue;
        }

        // Try burst read
        uint8_t received = i2cService.requestFrom(addr, toRead, true);

        if (received == 0) {
            // fallback, try single-byte read at this reg
            i2cService.beginTransmission(addr);
            if (!use16bitAddr) i2cService.write((uint8_t)(reg & 0xFF));
            else { i2cService.write((uint8_t)((reg >> 8) & 0xFF)); i2cService.write((uint8_t)(reg & 0xFF)); }
            if (i2cService.endTransmission(false) == 0) {
                received = i2cService.requestFrom(addr, (uint8_t)1, true);
            }
        }

        // Read whatever available
        for (uint8_t i = 0; i < received; ++i) {
            char key = terminalInput.readChar();
            if (key == '\r' || key == '\n') {
                terminalView.println("I2C Dump: Cancelled by user.");
                return;
            }

            if (i2cService.available()) {
                values[offset + i] = (uint8_t)i2cService.read();
                valid[offset + i] = true;
            } else {
                break;
            }
        }

        // Flush remaining just in case
        while (i2cService.available()) (void)i2cService.read();

        utilityService.sleepMs(1);
    }
}

void I2cController::performRawRead(uint8_t addr, uint16_t start,
                                   uint16_t len,
                                   std::vector<uint8_t>& values,
                                   std::vector<bool>& valid) {
    values.assign(len, 0xFF);
    valid.assign(len, false);

    terminalView.println("I2C Dump: Trying read raw...");

    const uint8_t CHUNK = 16;
    uint16_t pos = 0;

    (void)start;

    char key = terminalInput.readChar();
    if (key == '\r' || key == '\n') {
        terminalView.println("I2C Dump: Cancelled by user.");
        return;
    }

    // Probe without writing a register pointer so this fallback remains
    // compatible with devices that expose a non-register protocol.
    uint8_t got = i2cService.requestFrom(addr, (uint8_t)1, true);
    if (got == 0 || !i2cService.available()) return;

    values[pos] = (uint8_t)i2cService.read();
    valid[pos] = true;
    ++pos;

    while (pos < len) {
        key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("I2C Dump: Cancelled by user.");
            return;
        }

        uint8_t want = (uint8_t)((len - pos) > CHUNK ? CHUNK : (len - pos));

        got = i2cService.requestFrom(addr, want, true);
        if (got == 0) break;

        for (uint8_t i = 0; i < got && pos < len; ++i, ++pos) {
            if (i2cService.available()) {
                values[pos] = (uint8_t)i2cService.read();
                valid[pos] = true;
            } else {
                break;
            }
        }

        while (i2cService.available()) (void)i2cService.read();

        // If device consistently returns short, don't spin forever
        if (got < want) {
            break;
        }

        utilityService.sleepMs(1);
    }
}

void I2cController::printHexDump(uint16_t start, uint16_t len,
                                 const std::vector<uint8_t>& values, const std::vector<bool>& valid) {
    for (uint16_t lineStart = 0; lineStart < len; lineStart += 16) {
        std::string line;
        char addrStr[8];
        snprintf(addrStr, sizeof(addrStr), "%02X:", start + lineStart);
        line += addrStr;

        for (uint8_t i = 0; i < 16; ++i) {
            uint16_t idx = lineStart + i;
            if (idx < len) {
                if (valid[idx]) {
                    char hex[4];
                    snprintf(hex, sizeof(hex), " %02X", values[idx]);
                    line += hex;
                } else {
                    line += " ??";
                }
            } else {
                line += "   ";
            }
        }

        line += "  ";

        for (uint8_t i = 0; i < 16; ++i) {
            uint16_t idx = lineStart + i;
            if (idx < len && valid[idx]) {
                char c = values[idx];
                line += (c >= 32 && c <= 126) ? c : '.';
            } else {
                line += '.';
            }
        }

        terminalView.println(line);
    }
    terminalView.println("");
}

/*
Identify
*/
void I2cController::handleIdentify(const TerminalCommand& cmd) {
    uint8_t address = 0;
    if (!tryParseAddress(cmd.getSubcommand(), address)) {
        terminalView.println("Usage: identify <addr>");
        return;
    }
    terminalView.println(identifyToString(address, true));
}

/*
Recover
*/
void I2cController::handleRecover() {
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();
    uint32_t freq = state.getI2cFrequency();

    terminalView.println("I2C Reset: Attempting to recover I2C bus...");

    // Release I2C bus
    i2cService.end();
    // 16 clock pulse + STOP condition
    bool success = i2cService.i2cBitBangRecoverBus(scl, sda, freq);
    // Reconfigure I2C
    i2cService.configure(sda, scl, freq);

    if (success) {
        terminalView.println("\nI2C Reset: SDA released. Bus recovery successful.");
    } else {
        terminalView.println("\nI2C Reset: SDA still LOW after recovery, bus may remain stuck.");
    }
}

/*
Glitch
*/
void I2cController::handleGlitch(const TerminalCommand& cmd) {
    // Validate arg
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: glitch <addr>");
        return;
    }

    // Get I2C default config
    uint8_t scl = state.getI2cSclPin();
    uint8_t sda = state.getI2cSdaPin();
    uint32_t freqHz = state.getI2cFrequency();

    // Check I2C device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Glitch: No device found at " + cmd.getSubcommand());
        return;
    }

    terminalView.println("I2C Glitch: Attacking device at 0x" + argTransformer.toHex(addr) + "...\n");
    utilityService.sleepMs(500);

    terminalView.println(" 1. Flooding with random junk...");
    i2cService.floodRandom(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 2. Flooding START sequences...");
    i2cService.floodStart(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 3. Over-read (read more bytes than expected)...");
    i2cService.overReadAttack(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 4. Reading invalid/unmapped registers...");
    i2cService.invalidRegisterRead(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 5. Simulating clock stretch confusion...");
    i2cService.simulateClockStretch(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 6. Rapid START/STOP sequences...");
    i2cService.rapidStartStop(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 7. Glitching ACK phase...");
    i2cService.glitchAckInjection(addr, freqHz, scl, sda);
    utilityService.sleepMs(50);

    terminalView.println(" 8. Injecting random noise on SCL/SDA...");
    i2cService.randomClockPulseNoise(scl, sda, freqHz);
    utilityService.sleepMs(50);

    ensureConfigured();
    terminalView.println("\nI2C Glitch: Done. Target may be unresponsive or corrupted.");
}

/*
Flood
*/
void I2cController::handleFlood(const TerminalCommand& cmd) {
    // Validate arg
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: flood <addr>");
        return;
    }
    
    // Check device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Flood: No device found at " + cmd.getSubcommand());
        return;
    }
    
    terminalView.println("I2C Flood: Streaming read to 0x" + argTransformer.toHex(addr) + "... Press [ENTER] to stop.");
    while (true) {
        // Enter to stop
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\nI2C Flood: Stopped by user.");
            break;
        }

        // Random register address
        uint8_t reg = static_cast<uint8_t>(utilityService.randomUint32() & 0xFF);

        // Transmit only register
        i2cService.beginTransmission(addr);
        i2cService.write(reg);
        i2cService.endTransmission(true);
    }
}

/*
Jam
*/
void I2cController::handleJam() {
    uint8_t scl = state.getI2cSclPin();
    uint8_t sda = state.getI2cSdaPin();
    uint32_t freqHz = state.getI2cFrequency();

    terminalView.println("I2C Jam: Perturbing bus SCL/SDA... Press [ENTER] to stop.\n");

    // Release I2C bus
    i2cService.end();

    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        i2cService.injectRandomGlitch(scl, sda, freqHz);
    }

    // Try recovering bus after jamming
    i2cService.i2cBitBangRecoverBus(scl, sda, freqHz);

    // Reconfigure I2C
    ensureConfigured();
    terminalView.println("\nI2C Jam: Stopped by user.\n");
}

/*
Monitor
*/
void I2cController::handleMonitor(const TerminalCommand& cmd) {
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: monitor <addr> [delay_ms]");
        return;
    }
    uint16_t len = 256;
    uint32_t delayMs = 500;

    // Optional delay
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (!args.empty() && argTransformer.isValidNumber(args[0])) {
        delayMs = argTransformer.parseHexOrDec32(args[0]);
    }

    // Check device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Monitor: No device found at 0x" + argTransformer.toHex(addr));
        return;
    }

    terminalView.println("I2C Monitor: Monitoring register changes at 0x" + argTransformer.toHex(addr) + "... Press [ENTER] to stop.\n");

    std::vector<uint8_t> prev(len, 0xFF);
    std::vector<uint8_t> curr(len, 0xFF);
    std::vector<bool> valid(len, false);

    // First read to initialize prev
    if (i2cService.isReadableDevice(addr, 0x00)) {
        performRegisterRead(addr, 0x00, len, prev, valid);
    } else {
        performRawRead(addr, 0x00, len, prev, valid);
    }

    while (true) {
        // Try register read
        if (i2cService.isReadableDevice(addr, 0x00)) {
            performRegisterRead(addr, 0x00, len, curr, valid);
        } else {
            performRawRead(addr, 0x00, len, curr, valid);
        }

        // Compare and show changes
        for (uint16_t i = 0; i < len; ++i) {
            if (valid[i] && curr[i] != prev[i]) {
                std::stringstream ss;
                ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i
                   << ": 0x" << std::setw(2) << (int)prev[i]
                   << " -> 0x" << std::setw(2) << (int)curr[i];
                terminalView.println(ss.str());
                prev[i] = curr[i];
            }
        }

        // Check for user input to stop
        uint32_t elapsed = 0;
        while (elapsed < delayMs) {
            char key = terminalInput.readChar();
            if (key == '\r' || key == '\n') {
                terminalView.println("\nI2C Monitor: Stopped by user.");
                return;
            }
            utilityService.sleepMs(10);
            elapsed += 10;
        }
    }

    terminalView.println("\nI2C Monitor: Stopped.");
}

/*
Trace
*/
void I2cController::handleTrace(const TerminalCommand& cmd) {
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: trace <addr> [reg] [delay_ms]");
        return;
    }

    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (args.size() >= 2 && !argTransformer.isValidNumber(args[1])) {
        terminalView.println("Usage: trace <addr> [reg] [delay_ms]");
        return;
    }

    uint8_t reg = 0;
    if (args.size() >= 1) {
        if (!argTransformer.isValidNumber(args[0])) {
            terminalView.println("Error: Invalid register. Use decimal or 0x-prefixed hex.");
            return;
        }
        reg = argTransformer.parseHexOrDec(args[0]);
    } else {
        reg = (uint8_t)userInputManager.readValidatedByte("Register to trace", 0, true);
    }

    uint32_t delayMs = 500;
    if (args.size() >= 2) {
        delayMs = argTransformer.parseHexOrDec32(args[1]);
    }

    // Check device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Trace: No device found at 0x" + argTransformer.toHex(addr));
        return;
    }

    uint8_t prev = 0;
    if (!i2cService.readReg(addr, reg, &prev, nullptr)) {
        terminalView.println("I2C Trace: Unable to read reg 0x" + argTransformer.toHex(reg) +
                             " @ dev 0x" + argTransformer.toHex(addr) + ".");
        return;
    }

    terminalView.println("I2C Trace: Monitoring reg 0x" + argTransformer.toHex(reg) +
                         " @ dev 0x" + argTransformer.toHex(addr) +
                         "... Press [ENTER] to stop.\n");
    terminalView.println("Initial: 0x" + argTransformer.toHex(prev) +
                         " (" + argTransformer.toBinString(prev) + ")");

    while (true) {
        uint8_t curr = 0;
        if (i2cService.readReg(addr, reg, &curr, nullptr) && curr != prev) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)reg
               << ": 0x" << std::setw(2) << (int)prev
               << " (" << argTransformer.toBinString(prev) << ")"
               << " -> 0x" << std::setw(2) << (int)curr
               << " (" << argTransformer.toBinString(curr) << ")";
            terminalView.println(ss.str());
            prev = curr;
        }

        uint32_t elapsed = 0;
        while (elapsed < delayMs) {
            char key = terminalInput.readChar();
            if (key == '\r' || key == '\n') {
                terminalView.println("\nI2C Trace: Stopped by user.");
                return;
            }
            utilityService.sleepMs(10);
            elapsed += 10;
        }
    }
}

/*
EEPROM
*/
void I2cController::handleEeprom(const TerminalCommand& cmd) {
    uint8_t addr = 0x50; // Default EEPROM I2C address

    auto sub = cmd.getSubcommand();
    if (!sub.empty()) {
        if (!tryParseAddress(sub, addr)) {
            terminalView.println("Usage: eeprom [addr]");
            return;
        }
        if (addr < 0x03 || addr > 0x77) { // plage valide I2C 7-bit
            terminalView.println("❌ Invalid I2C address. Must be between 0x03 and 0x77.");
            return;
        }
    }

    eepromShell.run(addr);
    ensureConfigured();
}

/*
Help
*/
void I2cController::handleHelp() {
    terminalView.println("\nUnknown command. Available I2C commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Swap SDA and SCL pins
*/
void I2cController::handleSwap() {
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();

    // Swap in state
    state.setI2cSdaPin(scl);
    state.setI2cSclPin(sda);

    // Reconfigure I2C with swapped pins
    i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());

    terminalView.println(
        "I2C Swap: SDA/SCL swapped. SDA=" + std::to_string(state.getI2cSdaPin()) +
        " SCL=" + std::to_string(state.getI2cSclPin())
    );
    terminalView.println("");
}

/*
Health
*/
void I2cController::handleHealth(const TerminalCommand& cmd) {
    // Validate subcommand (addr)
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: health <addr>");
        return;
    }
    terminalView.println("I2C Health: Analyzing @ 0x" + argTransformer.toHex(addr) + "... Press [ENTER] to stop.\n");

    auto stoppedByUser = [&]() -> bool {
        char key = terminalInput.readChar();
        return (key == '\r' || key == '\n');
    };

    /* PING ANALYZE */
    terminalView.println("[ACK latency (ping)]");

    // Warm up
    uint32_t warmDt = 0;
    (void)i2cService.ping(addr, true, &warmDt);
    utilityService.sleepMs(1);

    const int PING_TRIES = 50;
    Stats ping;

    for (int i = 0; i < PING_TRIES; ++i) {
        if (stoppedByUser()) {
            terminalView.println("\nI2C Health: Stopped by user.");
            return;
        }

        uint32_t dt = 0;
        bool ok = i2cService.ping(addr, true, &dt);
        ping.add(ok, dt);

        utilityService.sleepMs(1);
    }

    if (ping.ok == 0) {
        terminalView.println("  ❌ No ACK received.\n");
        return;
    }

    terminalView.println("  Tries: " + std::to_string(PING_TRIES) +
                         "   Ok: " + std::to_string(ping.ok) +
                         "   Nack: " + std::to_string(ping.nack));
    terminalView.println("  Min: " + std::to_string(ping.minUs) + " µs   Avg: " +
                         std::to_string(ping.avgUs()) + " µs   Max: " +
                         std::to_string(ping.maxUs) + " µs\n\r  Jitter: " +
                         std::to_string(ping.jitterUs()) + " µs\n");

    /* REGISTER READ ANALYZE */
    terminalView.println("[Register read probe]");

    uint8_t probeVal = 0;
    uint32_t probeDt = 0;
    bool regSupported = i2cService.readReg(addr, 0x00, &probeVal, &probeDt);

    terminalView.println(std::string("  Supported: ") + (regSupported ? "yes" : "no\n"));

    const int READ_TRIES = 50;
    Stats rd;

    if (regSupported) {
        rd.add(true, probeDt);

        for (int i = 1; i < READ_TRIES; ++i) {
            if (stoppedByUser()) {
                terminalView.println("\nI2C Health: Stopped by user.");
                return;
            }

            uint8_t val = 0;
            uint32_t dt = 0;
            bool ok = i2cService.readReg(addr, 0x00, &val, &dt);
            rd.add(ok, dt);

            utilityService.sleepMs(1);
        }

        if (rd.ok > 0) {
            terminalView.println("  Tries: " + std::to_string(READ_TRIES) +
                                 "   Ok: " + std::to_string(rd.ok) +
                                 "   Nack: " + std::to_string(rd.nack));
            terminalView.println("  Min: " + std::to_string(rd.minUs) + " µs   Avg: " +
                                 std::to_string(rd.avgUs()) + " µs   Max: " +
                                 std::to_string(rd.maxUs) + " µs\n\r  Jitter: " +
                                 std::to_string(rd.jitterUs()) + " µs\n");
        } else {
            terminalView.println("  ❌ No successful register reads\n");
        }
    }

    /* REPORT */
    terminalView.println("[Health report for 0x" + argTransformer.toHex(addr) + "]");

    uint32_t freq = state.getI2cFrequency();
    uint32_t bitUs = (freq > 0) ? (1000000UL / freq) : 10; // fallback 10us

    uint32_t pingJ = ping.jitterUs();
    uint32_t readJ = (rd.ok > 0) ? rd.jitterUs() : 0;

    uint32_t pingThr = 20 * bitUs;
    uint32_t readThr = 40 * bitUs;

    bool pingStable = (ping.ok > 0) && (ping.nack == 0) && (pingJ <= pingThr);
    terminalView.println(pingStable ? "  ✅ Stable ACK" : "  ⚠️  ACK variability");

    if (!regSupported) {
        terminalView.println("  Skipped reads (non-standard device)");
    } else {
        bool readStable = (rd.ok > 0) && (rd.nack == 0) && (readJ <= readThr);
        terminalView.println(readStable ? "  ✅ Stable reads" : "  ⚠️  Read variability");
    }

    terminalView.println("  Freq=" + std::to_string(freq) + " Hz  bit=" + std::to_string(bitUs) + " µs");

    if (ping.nack == 0 && pingJ > (200 * bitUs)) {
        terminalView.println("  No NACKs but high latency variance.");
    }
    if (rd.ok > 0 && rd.nack == 0 && readJ > (300 * bitUs)) {
        terminalView.println("  Reads are reliable but variable.");
    }

    terminalView.println("");
}

/*
Regs
*/
void I2cController::handleRegs(const TerminalCommand& cmd) {
    // regs <addr> [len]
    uint8_t addr = 0;
    if (!tryParseAddress(cmd.getSubcommand(), addr)) {
        terminalView.println("Usage: regs <addr> [length]");
        return;
    }

    uint16_t start = 0x00;
    uint16_t len   = 256;

    // Parse optional len
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        len = argTransformer.parseHexOrDec16(args[0]);
    }

    // Limit
    if (len == 0) len = 1;
    if (len > 256) len = 256;
    uint16_t stop = (uint16_t)(start + len - 1);
    if (stop > 0xFF) stop = 0xFF;

    // Presence check
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C Regs: No device found at 0x" + argTransformer.toHex(addr));
        return;
    }

    terminalView.println("\n [⚠️  WARNING]");
    terminalView.println(" Regs will perform write/read tests,");
    terminalView.println(" to find which registers are writable.");
    terminalView.println(" Do NOT use on EEPROM or devices storing data.\n");

    // Confirm
    if (!userInputManager.readYesNo("Proceed registers probe?", false)) {
        terminalView.println("I2C Regs: Stopped by user.\n");
        return;
    }

    // Dump first (read)
    terminalView.println("");
    handleDump(cmd);

    // Probe the same range for writable registers
    terminalView.println("I2C Regs: probing 0x" + argTransformer.toHex(addr) +
                         " regs 0x" + argTransformer.toHex((uint8_t)start) +
                         "..0x" + argTransformer.toHex((uint8_t)stop) +
                         " ... Press [ENTER] to stop.\n");

    terminalView.println("[Writeable registers]");

    uint32_t tested = 0;
    uint32_t readable = 0;
    uint32_t writable = 0;
    uint8_t perLine = 8;
    uint8_t onLine = 0;
    std::string line = "  ";

    for (uint16_t r = start; r <= stop; ++r) {

        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\nI2C Regs: Stopped by user.\n");
            break;
        }

        uint8_t reg = (uint8_t)r;

        I2cRegProbeResult pr;
        bool ok = i2cService.probeRegRW(addr, reg, pr);
        if (!ok) {
            continue;
        }

        tested++;
        if (pr.readable) readable++;
        if (pr.rwHit)    writable++;

        if (pr.rwHit) {
            line += "0x" + argTransformer.toHex(reg);
            onLine++;

            if (onLine < perLine) line += "  ";

            if (onLine >= perLine) {
                terminalView.println(line);
                line = "  ";
                onLine = 0;
            }
        }

        utilityService.sleepMs(5);
    }

    // Flush last partial line
    if (onLine > 0 && line != "  ") {
        terminalView.println(line);
    }

    if (writable == 0) {
        terminalView.println(" No registers could be written.");
    }

    terminalView.println("");
    terminalView.println("[I2C Registers Summary]");
    terminalView.println(" Tested   : " + std::to_string(tested));
    terminalView.println(" Readable : " + std::to_string(readable));
    terminalView.println(" Writable : " + std::to_string(writable));
    terminalView.println("");
    
    // Ensure bus is not left in a bad state after potential failed writes
    for (int i = 0; i < 32; ++i) {
        utilityService.sleepMs(5);
        i2cService.endTransmission(true);
    }
}

void I2cController::handleDiscover() {
    auto stoppedByUser = [&]() -> bool {
        char key = terminalInput.readChar();
        return (key == '\r' || key == '\n');
    };

    terminalView.println("I2C Discover: scanning bus... Press [ENTER] to stop.\n");

    /* SCAN */
    std::vector<uint8_t> found;
    found.reserve(16);

    for (uint8_t addr = 1; addr < 0x7F; ++addr) {
        if (stoppedByUser()) {
            terminalView.println("\nI2C Discover: Stopped by user.");
            return;
        }

        uint32_t dt = 0;
        bool ok = i2cService.ping(addr, true, &dt);
        if (ok) {
            found.push_back(addr);
        }
        utilityService.sleepMs(1);
    }

    if (found.empty()) {
        terminalView.println("No I2C device detected.\n");
        return;
    }

    terminalView.println("Found " + std::to_string((int)found.size()) + " device(s):");
    for (auto a : found) {
        terminalView.println("  - 0x" + argTransformer.toHex(a));
    }
    terminalView.println("");

    /* IDENTIFY */
    const int PING_TRIES = 50;

    for (size_t idx = 0; idx < found.size(); ++idx) {
        uint8_t addr = found[idx];

        if (stoppedByUser()) {
            terminalView.println("\nI2C Discover: Stopped by user.");
            return;
        }

        terminalView.println("[" + std::to_string((int)(idx + 1)) + "/" +
                             std::to_string((int)found.size()) + "] Device @ 0x" +
                             argTransformer.toHex(addr));

        std::string info = identifyToString(addr, false);
        terminalView.println(info);

        // Ping timing
        // Warmup
        uint32_t warmDt = 0;
        (void)i2cService.ping(addr, true, &warmDt);
        utilityService.sleepMs(1);

        Stats s;
        for (int i = 0; i < PING_TRIES; ++i) {
            if (stoppedByUser()) {
                terminalView.println("\nI2C Discover: Stopped by user.");
                return;
            }

            uint32_t dt = 0;
            bool ok = i2cService.ping(addr, true, &dt);
            s.add(ok, dt);
            utilityService.sleepMs(1);
        }

        terminalView.println("  Ping timing (" + std::to_string(PING_TRIES) + "x):");
        terminalView.println("    Ok: " + std::to_string(s.ok) + "  Nack: " + std::to_string(s.nack));
        if (s.ok > 0) {
            terminalView.println("    Min: " + std::to_string(s.minUs) + " µs  Avg: " +
                                 std::to_string(s.avgUs()) + " µs  Max: " +
                                 std::to_string(s.maxUs) + " µs");
            terminalView.println("    Jitter: " + std::to_string(s.jitterUs()) + " µs");
        } else {
            terminalView.println("    ❌ No ACK during timing run");
        }

        terminalView.println("");
    }

    terminalView.println("I2C Discover: done.\n");
}

std::string I2cController::identifyToString(uint8_t address, bool includeHeader) {
    std::stringstream ss;

    if (includeHeader) {
        ss << "\n\r 📟 I2C 0x" + argTransformer.toHex(address) + " Identification Result\n";
    }

    bool matchFound = false;
    for (size_t i = 0; i < i2cknownAddressesCount; ++i) {
        if (i2cKnownAddresses[i].address == address) {
            matchFound = true;
            ss << "\r  ➤ Could be: - [" << i2cKnownAddresses[i].type << "] "
               << i2cKnownAddresses[i].component << "\n";
        }
    }

    if (!matchFound) {
        ss << "\r  ➤ No match found for address 0x" << argTransformer.toHex(address) << "\n";
    }

    return ss.str();
}

bool I2cController::tryParseAddress(const std::string& value, uint8_t& address) {
    if (!argTransformer.isValidNumber(value)) return false;

    const int base = value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0 ? 16 : 10;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, base);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' || parsed > 0x7F) {
        return false;
    }

    address = static_cast<uint8_t>(parsed);
    return true;
}

/*
Config
*/
void I2cController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        // Sniffer resources stay unallocated until the first sniff command.
        return;
    }

    // User could have set the same pin to a different usage
    // eg. select I2C then select UART then select I2C
    // Always reconfigure pins before use
    i2cService.end();
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();
    uint32_t freq = state.getI2cFrequency();
    i2cService.configure(sda, scl, freq);
}

/*
Release lazy I2C resources
*/
void I2cController::ensureReleased() {
    i2cSnifferService.release();
    std::string().swap(sniffLine); // release CLI text capacity on mode exit
    i2cService.end();
    configured = false;
}
