#include "JtagController.h"

/*
Constructor
*/
JtagController::JtagController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IJtagService& jtagService,
    UserInputManager& userInputManager,
    HelpShell& helpShell,
    IUsbAdapterShell& usbAdapterShell
) : terminalView(terminalView),
    terminalInput(terminalInput),
    jtagService(jtagService),
    userInputManager(userInputManager),
    helpShell(helpShell),
    usbAdapterShell(usbAdapterShell) {}
    
/*
Entry point that handles JTAG commands
*/
void JtagController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "scan") handleScan(cmd); 
    else if (cmd.getRoot() == "config") handleConfig();
    else if (cmd.getRoot() == "openocd") handleOpenOcd();
    else handleHelp();
}

/*
Scan
*/
void JtagController::handleScan(const TerminalCommand& cmd) {
    auto type = cmd.getSubcommand();

    if (type.empty()) terminalView.println("Usage: 'scan swd' or 'scan jtag'.");
    else if (type[0] == 's') handleScanSwd();
    else if (type[0] == 'j') handleScanJtag();
    else terminalView.println("Usage: 'scan swd' or 'scan jtag'.");
}

/*
Scan SWD
*/
void JtagController::handleScanSwd() {
    terminalView.println("JTAG: Scanning for SWD devices...");

    uint8_t swdio, swclk;
    uint32_t idcode;
    std::vector<uint8_t> swdCandidates = state.getJtagScanPins();

    bool found = jtagService.scanSwdDevice(swdCandidates, swdio, swclk, idcode);

    if (found) {
        char idcodeText[11];
        snprintf(idcodeText, sizeof(idcodeText), "0x%08X", idcode);
        terminalView.println("\n SWD device found!");
        terminalView.println("  • SWDIO  : GPIO " + std::to_string(swdio));
        terminalView.println("  • SWCLK  : GPIO " + std::to_string(swclk));
        terminalView.println("  • IDCODE : " + std::string(idcodeText));
        terminalView.println("  ✅ SWD scan done.\n");
    } else {
        terminalView.println("\nJTAG: No SWD device found on available GPIOs.");
    }
}

/*
Scan JTAG
*/
void JtagController::handleScanJtag() {    
    terminalView.println("JTAG: Scanning for JTAG devices...");

    std::vector<uint8_t> jtagCandidates = state.getJtagScanPins();
    uint8_t tdi, tdo, tck, tms;
    int trst;
    std::vector<uint32_t> ids;

    bool found = jtagService.scanJtagDevice(
        jtagCandidates,
        tdi, tdo, tck, tms, trst,
        ids,
        false, // blueTag default: keep active-low nTRST deasserted
        nullptr // callback progression
    );

    if (!found) {
        found = jtagService.scanJtagDevice(
            jtagCandidates,
            tdi, tdo, tck, tms, trst,
            ids,
            true, // retain pin pulsing as a fallback
            nullptr
        );
    }

    if (found) {
        terminalView.println("\n JTAG device(s) found!");
        terminalView.println("  • TDI   : GPIO " + std::to_string(tdi));
        terminalView.println("  • TDO   : GPIO " + std::to_string(tdo));
        terminalView.println("  • TCK   : GPIO " + std::to_string(tck));
        terminalView.println("  • TMS   : GPIO " + std::to_string(tms));
        if (trst >= 0) {
            terminalView.println("  • TRST  : GPIO " + std::to_string(trst));
        }

        for (size_t i = 0; i < ids.size(); ++i) {
            char buf[11];
            snprintf(buf, sizeof(buf), "0x%08X", ids[i]);
            terminalView.println("  • IDCODE[" + std::to_string(i) + "] : " + buf);
        }

        terminalView.println("  ✅ Scan complete.\n");
    } else {
        terminalView.println("\nJTAG: No device found on available GPIOs.");
    }
}

/*
Config
*/
void JtagController::handleConfig() {
    terminalView.println("JTAG/SWD Configuration:");

    // Default
    const std::vector<uint8_t> defaultPins = state.getJtagScanPins();

    // Protected
    const std::vector<uint8_t> protectedPins = state.getProtectedPins();

    // Ask user for pin group
    std::vector<uint8_t> selectedPins = userInputManager.readValidatedPinGroup(
        "GPIOs to scan (SWD/JTAG)",
        defaultPins,
        protectedPins
    );

    // Save it
    state.setJtagScanPins(selectedPins);

    // Show confirmation
    terminalView.print("GPIOs set for the scan (SWD/JTAG): ");
    for (uint8_t pin : selectedPins) {
        terminalView.print(std::to_string(pin) + " ");
    }
    terminalView.println("\r\nJTAG/SWD configured.\n");
}

/*
OpenOCD
*/
void JtagController::handleOpenOcd() {
    terminalView.println("\nOpenOCD requires a dedicated USB adapter mode.");
    terminalView.println("This will reboot into the OpenOCD Bus Pirate JTAG/SWD adapter.\n");

    auto confirm = userInputManager.readYesNo("Reboot into OpenOCD Bus Pirate adapter mode? (y/n)", false);
    if (!confirm) return;

    usbAdapterShell.rebootOpenOcdBusPirate();
}

/*
Help
*/
void JtagController::handleHelp() {
    terminalView.println("\nUnknown command. Available JTAG commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Ensure Configuration
*/
void JtagController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }
}
