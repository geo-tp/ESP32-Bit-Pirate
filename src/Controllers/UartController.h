#pragma once

#include <vector>
#include <queue>
#include <string>
#include "Models/TerminalCommand.h"
#include "Models/ByteCode.h"
#include "Interfaces/IUartService.h"
#include "Interfaces/IHdUartService.h"
#include "Interfaces/ISdService.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IDeviceView.h"
#include "Interfaces/IUtilityService.h"
#include "Interfaces/IShell.h"
#include "Interfaces/IUartSnifferService.h"
#include "States/GlobalState.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "Shells/HelpShell.h"

class UartController {
public:
    // Constructor
    UartController(ITerminalView& terminalView,
                   IInput& terminalInput,
                   IDeviceView& deviceView,
                   IInput& deviceInput,
                   IUtilityService& utilityService,
                   IUartService& uartService,
                   ISdService& sdService,
                   IHdUartService& hdUartService,
                   IUartSnifferService& uartSnifferService,
                   ArgTransformer& argTransformer,
                   UserInputManager& userInputManager,
                   IShell& uartAtShell,
                   HelpShell& helpShell,
                   IShell& uartEmulationShell);

    // Entry point for UART command
    void handleCommand(const TerminalCommand& cmd);

    //  Entry point for handle parsed bytecode instructions
    void handleInstruction(const std::vector<ByteCode>& bytecodes);

    // Ensure UART is configured before use
    void ensureConfigured();

    // Release UART resources (called on mode exit): end the serial
    // peripheral so RX/TX float (high-Z)
    void release();

private:
    // Start bidirectional UART bridge
    void handleBridge();

    // Perform ascii read
    void handleRead(const TerminalCommand& cmd);

    // Perform hex read
    void handleRaw();

    // Send probes to get a response
    void handlePing();

    // Write data to UART
    void handleWrite(const TerminalCommand& cmd);

    // Handle AT commands
    void handleAtCommand(const TerminalCommand& cmd);

    // Configure UART settings
    void handleConfig();

    // Display available commands
    void handleHelp();

    // Send glitch pattern over uart
    void handleGlitch();

    // Swap RX and TX pins
    void handleSwap();

    // Scan for active UART pins
    void handleScan();

    // Find the right baudrate
    void handleAutoBaud();

    // Spam given input every given ms
    void handleSpam(const TerminalCommand& cmd);

    // Xmodem cmd handler
    void handleXmodem(const TerminalCommand& cmd);

    // Receive file from xmodem
    void handleXmodemReceive(const std::string& path);

    // Send file to xmodem
    void handleXmodemSend(const std::string& path);

    // Handle UART emulation shell
    void handleEmulation();

    // Handle trigger setup to send response on pattern match
    void handleTrigger(const TerminalCommand& cmd);

    // sniff on a serial communication
    void handleSniff(const TerminalCommand& cmd);

    // sniff on a serial communication, text mode display
    void handleSniffTxt();

    // sniff on a serial communication, hex and ASCCI mode display
    void handleSniffRaw();

    ITerminalView& terminalView;
    IDeviceView& deviceView;
    IInput& terminalInput;
    IInput& deviceInput;
    IUtilityService& utilityService;
    IUartService& uartService;
    ISdService& sdService;
    IHdUartService& hdUartService;
    IUartSnifferService& uartSnifferService;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    IShell& uartAtShell;
    HelpShell& helpShell;
    IShell& uartEmulationShell;
    GlobalState& state = GlobalState::getInstance();
    bool configured = false;
    bool scanCancelled = false;

    // Predefined probe commands for ping
    inline static constexpr const char* const kProbes[] = {
        // Generic AT commands
        "AT\r\n",
        "ATE0\r\n", "ATE1\r\n", "ATI\r\n", "ATI0\r\n", "ATI1\r\n", "ATI2\r\n",
        "AT+GMR\r\n", "AT+VERSION?\r\n", "AT+HELP\r\n",

        // Modem/GSM specific
        "AT+CSQ\r\n", "AT+CREG?\r\n", "AT+CGMI\r\n", "AT+CGMM\r\n",
        "AT+CGMR\r\n", "AT+CGSN\r\n",

        // GPS modules
        "$PMTK?\r\n", "$GPTXT\r\n",

        // Miscellaneous
        "help\r\n", "?\r\n", "\r\n",
        "\x1B", "\x03", "\x04", "\x1A", "\x11", "\x13"  // ESC, Ctrl+C/Z/D, XON/XOFF
    };
    inline static constexpr size_t kProbesCount = sizeof(kProbes) / sizeof(kProbes[0]);
};
