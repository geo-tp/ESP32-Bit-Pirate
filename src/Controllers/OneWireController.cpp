#include "OneWireController.h"
#include <cstring>
#include <cstdio>

namespace {
constexpr int kSignalLow = 0;
constexpr int kSignalHigh = 1;
}

/*
Constructor
*/
OneWireController::OneWireController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IUtilityService& utilityService,
    IOneWireService& service,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    IShell& ibuttonShell,
    IShell& eepromShell,
    HelpShell& helpShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      utilityService(utilityService),
      oneWireService(service),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      ibuttonShell(ibuttonShell),
      eepromShell(eepromShell),
      helpShell(helpShell) {
}

/*
Entry point for command
*/
void OneWireController::handleCommand(const TerminalCommand& command) {
    if (command.getRoot()      == "scan")   handleScan();
    else if (command.getRoot() == "ping")   handlePing();
    else if (command.getRoot() == "xfer")   handleXfer(command);
    else if (command.getRoot() == "sniff")  handleSniff();
    else if (command.getRoot() == "read")   handleRead();
    else if (command.getRoot() == "write")  handleWrite(command);
    else if (command.getRoot() == "ibutton")   handleIbutton(command);
    else if (command.getRoot() == "eeprom")   handleEeprom();
    else if (command.getRoot() == "temp")   handleTemperature();
    else if (command.getRoot() == "config") handleConfig();
    else                                    handleHelp();
}

/*
Entry point for instructions
*/
void OneWireController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = oneWireService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("OneWire Read:\n");
        terminalView.println(result);
    }
}

/*
Scan
*/
void OneWireController::handleScan() {
    terminalView.println("OneWire Scan: in progress...");

    oneWireService.resetSearch();

    uint8_t rom[8];
    int deviceCount = 0;

    while (oneWireService.search(rom)) {
        std::ostringstream oss;
        oss << "Device " << (++deviceCount) << ": ";
        for (int i = 0; i < 8; ++i) {
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                << static_cast<int>(rom[i]) << " ";
        }

        uint8_t crc = oneWireService.crc8(rom, 7);
        if (crc != rom[7]) {
            oss << "(CRC error)";
        }

        terminalView.println(oss.str());
    }

    if (deviceCount == 0) {
        terminalView.println("OneWire Scan: No devices found.");
    }
}

/*
Ping
*/
void OneWireController::handlePing() {
    bool devicePresent = oneWireService.reset();
    if (devicePresent) {
        terminalView.println("OneWire Ping: Device present.");
    } else {
        terminalView.println("OneWire Ping: No device found.");
    }
}

// xfer <read_count> [0xB1 ... 0xBn]
// Reset, master-write the given bytes (8 write slots each), then read
// read_count bytes (8 read slots each). Prints "1WIRE XFER: <hex bytes>".
void OneWireController::handleXfer(const TerminalCommand& command) {
    // First positional arrives as the subcommand (see I2C handlers); the
    // remainder comes via getArgs().
    std::vector<std::string> args;
    if (!command.getSubcommand().empty()) {
        args.push_back(command.getSubcommand());
    }
    auto rest = argTransformer.splitArgs(command.getArgs());
    for (const auto& a : rest) {
        args.push_back(a);
    }
    if (args.empty()) {
        terminalView.println("Usage: xfer <read_count> [0xB1 ... 0xBn]");
        return;
    }
    if (!argTransformer.isValidNumber(args[0])) {
        terminalView.println("Error: Invalid read count. Use decimal or 0x-prefixed hex.");
        return;
    }
    uint8_t readCount = (uint8_t)argTransformer.parseHexOrDec(args[0]);
    std::vector<uint8_t> writeBytes;
    for (size_t i = 1; i < args.size(); i++) {
        if (!argTransformer.isValidNumber(args[i])) {
            terminalView.println("Error: Invalid byte. Use decimal or 0x-prefixed hex.");
            return;
        }
        writeBytes.push_back((uint8_t)argTransformer.parseHexOrDec(args[i]));
    }
    if (!oneWireService.reset()) {
        terminalView.println("1WIRE XFER: no device");
        return;
    }
    for (uint8_t b : writeBytes) {
        oneWireService.write(b);
    }
    if (readCount == 0) {
        terminalView.println("1WIRE XFER: 0 bytes read");
        return;
    }
    std::string out = "1WIRE XFER:";
    for (uint8_t i = 0; i < readCount; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), " %02X", oneWireService.read());
        out += buf;
    }
    terminalView.println(out);
}

/*
Read
*/
void OneWireController::handleRead() {
    terminalView.println("OneWire Read: Press [ENTER] to stop.\n");

    while (true) {
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("OneWire Read: Stopped by user.");
            break;
        }

        auto idReaded = handleIdRead();
        auto spReaded = handleScratchpadRead();

        if (idReaded && spReaded) {
            terminalView.println("OneWire Read: Complete.");
            terminalView.println("");
            break;
        }
        utilityService.sleepMs(100);
    }
}

/*
ID Read
*/
bool OneWireController::handleIdRead() {
    uint8_t buffer[8];

    if (!oneWireService.reset()) {
        return false;
    }
    terminalView.println("OneWire Read: in progress.");

    oneWireService.write(0x33);  // Read ROM
    oneWireService.readBytes(buffer, 8);

    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        oss << std::setw(2) << static_cast<int>(buffer[i]);
        if (i < 7) oss << " ";
    }

    terminalView.println("ROM ID: " + oss.str());

    uint8_t crc = oneWireService.crc8(buffer, 7);
    if (crc != buffer[7]) {
        terminalView.println("OneWire Read:: CRC error on ROM ID.");
    }

    return true;
}

/*
Scratchpad Read
*/
bool OneWireController::handleScratchpadRead() {
    uint8_t scratchpad[8];
    if (!oneWireService.reset()) {
        return false;
    }

    oneWireService.write(0xAA);  // Read Scratchpad
    oneWireService.readBytes(scratchpad, 8);

    std::ostringstream oss;
    oss << "Scratchpad: ";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)scratchpad[i];
        if (i < 7) oss << " ";
    }
    terminalView.println(oss.str());

    // CRC (last byte)
    uint8_t crc = oneWireService.crc8(scratchpad, 7);
    if (crc != scratchpad[7]) {
        terminalView.println("CRC error on scratchpad.");
    }

    return true;
}

/*
Write
*/
void OneWireController::handleWrite(const TerminalCommand& cmd) {
    std::string sub = cmd.getSubcommand();

    if (sub == "id" || sub == "sp") {
        std::vector<uint8_t> bytes;

        // Bytes are given in command line
        if (!cmd.getArgs().empty()) {
            bytes = argTransformer.parseByteList(cmd.getArgs());
        } else {
            // Interactive input if no arguments are given
            std::string prompt = "Enter 8-byte (28 FF AA...) ";

            std::string input = userInputManager.readValidatedHexString(prompt, 8, false);
            bytes = argTransformer.parseHexList(input);
        }

        // Length check
        if (bytes.size() != 8) {
            terminalView.println("OneWire Write: Must be exactly 8 bytes.");
            return;
        }

        if (sub == "id") {
            handleIdWrite(bytes);
        } else {
            handleScratchpadWrite(bytes);
        }
    }

    else {
        terminalView.println("OneWire Write: Invalid syntax. Use:");
        terminalView.println("  write id [8 bytes]");
        terminalView.println("  write sp [8 bytes]");
    }
}

/*
ID Write
*/
void OneWireController::handleIdWrite(std::vector<uint8_t> idBytes) {
    const int maxRetries = 8;
    int attempt = 0;
    bool success = false;

    terminalView.println("OneWire ID Write: Waiting for device... Press [ENTER] to stop");

    // Wait detection
    while (!oneWireService.reset()) {
        utilityService.sleepMs(1);
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("OneWire Write: Stopped by user.");
            return;
        }
    }

    // Try to write and verify X times
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("Attempt " + std::to_string(attempt) + "...");

        // Write
        oneWireService.writeRw1990(state.getOneWirePin(), idBytes.data(), idBytes.size());
        utilityService.sleepMs(50);

        // Read ID to verify
        uint8_t buffer[8];
        if (!oneWireService.reset()) continue;
        oneWireService.write(0x33); // Read ROM
        oneWireService.readBytes(buffer, 8);

        // Read is not equal to given one
        if (memcmp(buffer, idBytes.data(), 7) != 0) {
            terminalView.println("Mismatch in ROM ID bytes.");
            continue;
        }

        success = true;
        break;
    }

    if (success) {
        terminalView.println("OneWire Write: ID write successful.");
    } else {
        terminalView.println("OneWire Write: Failed to write.");
    }
}

/*
Scratchpad Write
*/
void OneWireController::handleScratchpadWrite(std::vector<uint8_t> scratchpadBytes) {
    const int maxRetries = 8;
    int attempt = 0;
    bool success = false;

    terminalView.println("OneWire Write: Waiting for device... Press [ENTER] to stop");

    // Wait for device presence
    while (!oneWireService.reset()) {
        auto c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            terminalView.println("Aborted by user.");
            return;
        }
        utilityService.sleepMs(1);
    }

    // Try up to X times
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("Attempt " + std::to_string(attempt) + "...");

        if (!oneWireService.reset()) continue;

        oneWireService.skip();
        oneWireService.write(0x0F); // Scratchpad write command
        utilityService.sleepUs(20);
        oneWireService.writeBytes(scratchpadBytes.data(), 8);
        utilityService.sleepMs(50);

        // Verify by reading back
        if (!oneWireService.reset()) continue;

        oneWireService.skip();
        oneWireService.write(0xAA); // Read Scratchpad
        utilityService.sleepUs(20);

        uint8_t readback[8];
        oneWireService.readBytes(readback, 8);

        // Missmatch
        if (memcmp(readback, scratchpadBytes.data(), 8) != 0) {
            terminalView.println("Mismatch in scratchpad data.");
            continue;
        }

        // CRC error
        uint8_t crc = oneWireService.crc8(readback, 8);
        if (crc != readback[7]) {
            terminalView.println("CRC error on scratchpad.");
            continue;
        }

        success = true;
    }

    if (success) {
        terminalView.println("OneWire Write: Scratchpad write successful.");
    } else {
        terminalView.println("OneWire Write: Failed after 3 attempts.");
    }
}

/*
iButton
*/
void OneWireController::handleIbutton(const TerminalCommand& command) {
    ibuttonShell.run();
}

/*
Config
*/
void OneWireController::handleConfig() {
    terminalView.println("OneWire Configuration:");

    const auto& forbidden = state.getProtectedPins();

    uint8_t pin = userInputManager.readValidatedPinNumber("Data GPIO", state.getOneWirePin(), forbidden);
    state.setOneWirePin(pin);
    oneWireService.configure(pin);

    terminalView.println("OneWire configured.");
    terminalView.println("");
}

/*
Sniff
*/
void OneWireController::handleSniff() {
    terminalView.println("OneWire Sniff: Oberserving data line... Press [ENTER] to stop.\n");

    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        terminalView.println(" [ℹ️  INFORMATION] ");
        terminalView.println(" This feature uses very fast timing.");
        terminalView.println(" The Web CLI may miss some signals,");
        terminalView.println(" use Serial CLI for best results.\n");
    }

    // Init the pin to read passively
    oneWireService.beginPassiveSniff();

    // Read initial state of pin
    int prev = oneWireService.readPinLevel();
    uint32_t lastFall = utilityService.nowUs();

    while (true) {
        // Enter press
        auto c = terminalInput.readChar();
        if (c == '\r' || c == '\n' ) break;

        // Read current state of pin
        int current = oneWireService.readPinLevel();
        uint32_t now = utilityService.nowUs();

        // Detect a falling edge
        if (prev == kSignalHigh && current == kSignalLow) {
            lastFall = now;
        }

        // Detect a rising edge (end of a LOW pulse)
        if (prev == kSignalLow && current == kSignalHigh) {
            // Calculate duration of the LOW pulse
            uint32_t duration = now - lastFall;

            // Too long, not mean anything
            if (duration >= 3000) {
                terminalView.println("[Non-Standard Pulse] " + std::to_string(duration) + " µs");
            }
            // Most likely a reset pulse
            else if (duration >= 480) {
                terminalView.println("[Reset] LOW for " + std::to_string(duration) + " µs");

            // Most likely a presence pulse
            } else if (duration >= 60 && duration <= 240) {
                terminalView.println("[Presence] LOW for " + std::to_string(duration) + " µs");

            // Most likely a bit transmission
            } else if (duration >= 10 && duration <= 70) {
                // Want to sample ~15 µs after the falling edge
                // which is the standard sampling time in the 1-Wire protocol
                uint32_t elapsed = now - lastFall;
                if (elapsed < 15) {
                    utilityService.sleepUs(15 - elapsed);
                }

                // Read the bit
                int sample = oneWireService.readPinLevel();
                terminalView.println("[Bit] LOW " + std::to_string(duration) +
                                     " µs, Sample = " + std::to_string(sample));

            // Unrecognized or noisy signal
            } else {
                terminalView.println("[Noise] LOW for " + std::to_string(duration) + " µs");
            }
        }

        // Update
        prev = current;
    }
    terminalView.println("\n\nOneWire Sniff: Stopped by user.");
}

/*
Temp
*/
void OneWireController::handleTemperature() {
    terminalView.println("OneWire Temp: Searching for DS18B20...");

    uint8_t rom[8];
    bool found = false;

    oneWireService.resetSearch();

    // Search captor type 0x28
    while (oneWireService.search(rom)) {
        if (rom[0] == 0x28) { // DS18B20
            found = true;
            break;
        }
    }

    if (!found) {
        terminalView.println("OneWire Temp: No DS18B20 device found.");
        return;
    }

    // Display ID
    std::ostringstream oss;
    oss << "\nDS18B20 ROM: ";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(rom[i]) << " ";
    }
    terminalView.println(oss.str());

    // Start temp read process
    if (!oneWireService.reset()) {
        terminalView.println("OneWire Temp: Reset failed.");
        return;
    }

    // Select and write CONVERT T
    oneWireService.select(rom);
    oneWireService.write(0x44);  // CONVERT T
    utilityService.sleepMs(750); // wait conversion max 750ms

    if (!oneWireService.reset()) {
        terminalView.println("OneWire Temp: Reset failed before scratchpad read.");
        return;
    }

    // Read sratchpad
    oneWireService.select(rom);
    oneWireService.write(0xBE); // READ SCRATCHPAD

    uint8_t data[9];
    oneWireService.readBytes(data, 9);

    // CRC check
    uint8_t crc = oneWireService.crc8(data, 8);
    if (crc != data[8]) {
        terminalView.println("OneWire Temp: CRC error in scratchpad.");
        return;
    }

    // Extract temp
    int16_t raw = (data[1] << 8) | data[0];
    float tempC = raw / 16.0f;

    // Display converted temp
    std::ostringstream tempStr;
    tempStr << "Temperature: " << std::fixed << std::setprecision(2) << tempC << " °C\n";
    terminalView.println(tempStr.str());
}

/*
EEPROM
*/
void OneWireController::handleEeprom() {
    terminalView.println("OneWire EEPROM: EEPROM Shell starts...");
    oneWireService.close();
    oneWireService.configureEeprom(state.getOneWirePin());
    eepromShell.run();
    oneWireService.closeEeprom();
    ensureConfigured();
}

/*
Help
*/
void OneWireController::handleHelp() {
    terminalView.println("\nUnknown command. Available 1Wire commands:");
    helpShell.run(state.getCurrentMode(), false);
}

void OneWireController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    uint8_t pin = state.getOneWirePin();
    oneWireService.configure(pin);
}
