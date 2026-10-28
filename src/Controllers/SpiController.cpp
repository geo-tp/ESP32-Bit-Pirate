#include "SpiController.h"
#include <algorithm>

/*
Constructor
*/
SpiController::SpiController(ITerminalView& terminalView, IInput& terminalInput, 
                             IUtilityService& utilityService, ISpiService& spiService, ISdService& sdService, ArgTransformer& argTransformer,
                             UserInputManager& userInputManager, BinaryAnalyzer& binaryAnalyzer,
                             IShell& sdCardShell, IShell& spiFlashShell, IShell& spiEepromShell,
                             HelpShell& helpShell)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      utilityService(utilityService),
      spiService(spiService),
      sdService(sdService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      binaryAnalyzer(binaryAnalyzer),
      sdCardShell(sdCardShell),
      spiFlashShell(spiFlashShell),
      spiEepromShell(spiEepromShell),
      helpShell(helpShell)
{}

/*
Entry point for command
*/
void SpiController::handleCommand(const TerminalCommand& cmd) {
    if      (cmd.getRoot() == "sniff")  handleSniff();
    else if (cmd.getRoot() == "sdcard") handleSdCard();
    else if (cmd.getRoot() == "slave")  handleSlave();
    else if (cmd.getRoot() == "flash")  handleFlash(cmd);
    else if (cmd.getRoot() == "eeprom") handleEeprom(cmd);
    else if (cmd.getRoot() == "help")   handleHelp();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point for instructions
*/
void SpiController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = spiService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("SPI Read:\n");
        terminalView.println(result);
    }
}

/*
Sniff
*/
void SpiController::handleSniff() {
    // Select line
    std::vector<std::string> choices = { " MOSI", " MISO" };
    int choice = userInputManager.readValidatedChoiceIndex("Select line to sniff", choices, 0);
    bool sniffMosi = (choice == 0);

    // Pins
    int sclk = state.getSpiCLKPin();
    int miso = state.getSpiMISOPin();
    int mosi = state.getSpiMOSIPin();
    int cs   = state.getSpiCSPin();

    // Release SPI if in use
    spiService.end();

    // Mapping 
    int slaveMisoPin = sniffMosi ? miso : -1;
    int slaveMosiPin = sniffMosi ? mosi : miso;

    terminalView.println("SPI Sniffer: In progress... Press [ENTER] to stop.");

    terminalView.println("");
    terminalView.println(" [ℹ️  INFORMATION]");
    terminalView.println(" SPI Sniff mode listens passively on the bus.");
    terminalView.println(" Connect SCK, MOSI, MISO and CS lines.");
    terminalView.println(" Data is only captured when CS is active.");
    terminalView.println("");

    // Launch SPI slave on the selected line
    spiService.startSlave(sclk, slaveMisoPin, slaveMosiPin, cs);

    // Log data until user stops
    const char* tag = sniffMosi ? "[MOSI] " : "[MISO] ";
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        auto packets = spiService.getSlaveData();
        for (const auto& packet : packets) {
            if (packet.empty()) continue;
            std::stringstream ss;
            ss << tag;
            for (uint8_t b : packet) {
                ss << std::hex << std::uppercase
                   << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            terminalView.println(ss.str());
        }
    }

    terminalView.println("\nSPI Sniffer: Stopping... Please wait.");
    spiService.stopSlave(sclk, slaveMisoPin, slaveMosiPin, cs);
    spiService.end();
    spiService.configure(mosi, miso, sclk, cs, state.getSpiFrequency(), state.getSpiWPPin(), state.getSpiHOLDPin());
    terminalView.println("SPI Sniffer: Stopped by user.\n");
}

/*
Flash
*/
void SpiController::handleFlash(const TerminalCommand& cmd) {
    spiFlashShell.run();
}

/*
EEPROM
*/
void SpiController::handleEeprom(const TerminalCommand& cmd) {
    spiEepromShell.run();
    ensureConfigured();
}

/*
Slave
*/
void SpiController::handleSlave() {
    spiService.end(); // Stop master mode if active
    
    int sclk = state.getSpiCLKPin();
    int miso = state.getSpiMISOPin();
    int mosi = state.getSpiMOSIPin();
    int cs   = state.getSpiCSPin();

    terminalView.println("SPI Slave: In progress... Press [ENTER] to stop.");
    spiService.startSlave(sclk, miso, mosi, cs);

    terminalView.println("");
    terminalView.println(" [ℹ️  INFORMATION]");
    terminalView.println(" SPI Slave mode listens passively on the SPI bus.");
    terminalView.println(" Any command sent by a master will be captured");
    terminalView.println(" Data is only captured when CS is active.");
    terminalView.println("");

    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Read slave data from master
        auto packets = spiService.getSlaveData();
        for (const auto& packet : packets) {
            std::stringstream ss;
            ss << "[MOSI] ";
            for (uint8_t b : packet) {
                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            terminalView.println(ss.str());
        }
    }
    terminalView.println("\nSPI Slave: Stopping... Please wait.");
    spiService.stopSlave(sclk, miso, mosi, cs);
    spiService.end();
    spiService.configure(mosi, miso, sclk, cs, state.getSpiFrequency(), state.getSpiWPPin(), state.getSpiHOLDPin());
    terminalView.println("SPI Slave: Stopped by user.\n");
}

/*
SD Card
*/
void SpiController::handleSdCard() {
    uint8_t cs, clk, miso, mosi;
    cs = state.getSpiCSPin();
    clk = state.getSpiCLKPin();
    miso = state.getSpiMISOPin();
    mosi = state.getSpiMOSIPin();

    // Internal SD card slot
    if (state.getHasInternalSdCard()) {
        auto confirm = userInputManager.readYesNo("Use internal SD card slot?", true);
        if (confirm) {
            cs = state.getSdCardCsPin();
            clk = state.getSdCardClkPin();
            miso = state.getSdCardMisoPin();
            mosi = state.getSdCardMosiPin();
        }
    }

    terminalView.println("SD Card: Mounting...");
    utilityService.sleepMs(500);

    // Configure
    spiService.end();
    bool success = sdService.configure(
        clk,
        miso,
        mosi,
        cs
    );

    if (!success) {
        terminalView.println("SD Card: Mount failed. Check config and wiring and try again.\n");
        sdService.end();
        spiService.configure(state.getSpiMOSIPin(), state.getSpiMISOPin(), state.getSpiCLKPin(),
                             state.getSpiCSPin(), state.getSpiFrequency(), state.getSpiWPPin(), state.getSpiHOLDPin());
        return;
    }
    
    // SD Shell
    terminalView.println("SD Card: Mounted successfully. Loading...\n");
    sdCardShell.run();

    // Reconfigure
    sdService.end();
    spiService.end();
    ensureConfigured();
}

/*
Help
*/
void SpiController::handleHelp() {
    terminalView.println("\nUnknown command. Available SPI commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Config
*/
void SpiController::handleConfig() {
    terminalView.println("SPI Configuration:");

    auto forbidden = state.getProtectedPins();

    uint8_t mosi = userInputManager.readValidatedPinNumber("MOSI GPIO", state.getSpiMOSIPin(), forbidden);
    state.setSpiMOSIPin(mosi);
    forbidden.push_back(mosi);

    uint8_t miso = userInputManager.readValidatedPinNumber("MISO GPIO", state.getSpiMISOPin(), forbidden);
    state.setSpiMISOPin(miso);
    forbidden.push_back(miso);

    uint8_t sclk = userInputManager.readValidatedPinNumber("SCLK GPIO", state.getSpiCLKPin(), forbidden);
    state.setSpiCLKPin(sclk);
    forbidden.push_back(sclk);

    uint8_t cs = userInputManager.readValidatedPinNumber("CS GPIO", state.getSpiCSPin(), forbidden);
    state.setSpiCSPin(cs);
    forbidden.push_back(cs);

    uint32_t freqMhz = state.getSpiFrequency() / 1000000;
    uint32_t freq = userInputManager.readValidatedUint8("Frequency (MHz)", freqMhz, 1, 80) * 1000000;
    state.setSpiFrequency(freq);

    const auto selectControlPin = [&](const std::string& name) -> int8_t {
        while (true) {
            const int pin = userInputManager.readValidatedInt(name + " GPIO", -1, -1, 48);
            if (pin == -1) return -1;
            if (std::find(forbidden.begin(), forbidden.end(), pin) != forbidden.end()) {
                terminalView.println("This GPIO is reserved/protected and cannot be used.");
                continue;
            }
            forbidden.push_back(static_cast<uint8_t>(pin));
            return static_cast<int8_t>(pin);
        }
    };
    int8_t wp = -1;
    int8_t hold = -1;
    if (userInputManager.readYesNo("Use WP or HOLD ?", false)) {
        wp = selectControlPin("WP");
        hold = selectControlPin("HOLD");
    }
    state.setSpiWPPin(wp);
    state.setSpiHOLDPin(hold);
    spiService.configure(mosi, miso, sclk, cs, freq, wp, hold);
    if (wp >= 0) terminalView.println("WP held HIGH; flash status-register bits are unchanged.");
    if (hold >= 0) terminalView.println("HOLD held HIGH (inactive).");

    terminalView.println("SPI configured.\n");
}

/*
Ensure SPI is configured
*/
void SpiController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }

    spiService.end();
    sdService.end();
    // Reconfigure, user could have used these pins for another mode
    uint8_t sclk = state.getSpiCLKPin();
    uint8_t miso = state.getSpiMISOPin();
    uint8_t mosi = state.getSpiMOSIPin();
    uint8_t cs   = state.getSpiCSPin();
    int8_t wp    = state.getSpiWPPin();
    int8_t hold  = state.getSpiHOLDPin();
    int freq   = state.getSpiFrequency();
    spiService.configure(mosi, miso, sclk, cs, freq, wp, hold);
}
