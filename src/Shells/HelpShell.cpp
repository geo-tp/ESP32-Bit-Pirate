#include "HelpShell.h"

HelpShell::HelpShell(ITerminalView& tv,
                     IInput& in,
                     UserInputManager& uim)
    : terminalView(tv)
    , terminalInput(in)
    , userInputManager(uim) {}


void HelpShell::run(ModeEnum currentMode, bool select) {
    auto actions = ModeEnumMapper::getProtocols();
    auto actionsStr = ModeEnumMapper::getProtocolNames(actions);
    actionsStr[0] = "ALL"; // hiz as all cmds
    actionsStr.push_back("Exit");

    // Find default index for current mode
    int defaultIndex =actionsStr.size() - 1; // Exit by default
    for (size_t i = 0; i < actions.size(); ++i) {
        if (actions[i] == currentMode) {
            defaultIndex = (int)i;
            break;
        }
    }

    // Alignement
    for (size_t i = 0; i < actionsStr.size(); ++i) {
        if (i + 1 < 10)
            actionsStr[i] = "  " + actionsStr[i];
        else
            actionsStr[i] = " " + actionsStr[i];
    }

    // Ask index
    int choice = defaultIndex;
    if (select) {
        terminalView.println("\n=== Help Shell: Commands by Mode ===\n");
        choice = userInputManager.readValidatedChoiceIndex(
            "Select a mode to get help",
            actionsStr,
            defaultIndex // last option is exit
        );
    }

    if (choice != actionsStr.size() - 1) // not exit
         cmdGeneral(); // print general cmds for all modes

    switch (choice) {
        case 0:  cmdAll();       break;
        case 1:  cmdOneWire();   break;
        case 2:  cmdUart();      break;
        case 3:  cmdHdUart();    break;
        case 4:  cmdI2c();       break;
        case 5:  cmdSpi();       break;
        case 6:  cmdTwoWire();   break;
        case 7:  cmdThreeWire(); break;
        case 8:  cmdDio();       break;
        case 9:  cmdLed();       break;
        case 10: cmdInfrared();  break;
        case 11: cmdUsb();       break;
        case 12: cmdBluetooth(); break;
        case 13: cmdWifi();      break;
        case 14: cmdJtag();      break;
        case 15: cmdI2s();       break;
        case 16: cmdCan();       break;
        case 17: cmdEthernet();  break;
        case 18: cmdSubGhz();    break;
        case 19: cmdRfid();      break;
        case 20: cmdRf24();      break;
        case 21: cmdFm();        break;
        case 22: cmdCell();      break;
        case 23: cmdLoRa();      break;
        case 24: cmdExpander();  break;
        default:
            break;
    }

    terminalView.println("");
}

void HelpShell::printHeader(const char* title) {
    terminalView.println("");
    terminalView.println("====================================");
    terminalView.println(title);
    terminalView.println("====================================");
    terminalView.println("");
}

void HelpShell::printLines(const char* const* lines, int count) {
    for (int i = 0; i < count; i++) {
        terminalView.println(lines[i]);
    }
}

/* ==========================
   Sections
   ========================== */

void HelpShell::cmdGeneral() {
    printHeader("General");

    static const char* const lines[] = {
        "help                 - Show this help",
        "mode [name]          - Set active mode",
        "man                  - Show firmware guide",
        "system               - Show system infos",
        "profile              - Save/load GPIOs config",
        "alias                - Create shortcut",
        "hex [number]         - Convert dec/hex/bin",
        "logic <gpio>         - Logic analyzer",
        "analogic <gpio>      - Analogic plotter",
        "wizard <gpio>        - GPIO activity analyzer",
        "listen <gpio>        - GPIO activity to audio",
        "repeat <count> <cmd> - Repeat command",
        "P                    - Enable pull-up",
        "p                    - Disable pull-up",
        "",
        "Tip: use 'mode' (or 'm')."
    };

    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdOneWire() {
    printHeader("1WIRE");
    static const char* const lines[] = {
        "scan                 - Scan 1-Wire devices",
        "ping                 - Ping 1-Wire device",
        "sniff                - View 1-Wire traffic",
        "read                 - Read ID + SP",
        "write id [8 bytes]   - Write device ID",
        "write sp [8 bytes]   - Write scratchpad",
        "temp                 - Read temperature",
        "ibutton              - iButton operations",
        "eeprom               - EEPROM operations",
        "config               - Configure settings",
        "[0xAA r:8] ...       - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdUart() {
    printHeader("UART");
    static const char* const lines[] = {
        "scan                 - Scan UART lines activity",
        "autobaud             - Detect baud rate on RX",
        "ping                 - Send and expect reply",
        "sniff                - Monitor ascii UART traffic",
        "sniff raw            - Monitor hex UART traffic",
        "read                 - Receive ascii data",
        "raw                  - Receive raw hex data",
        "write [text]         - Send at current baud",
        "bridge               - Full-duplex mode",
        "at                   - AT commands operations",
        "emulator             - Emulate UART device",
        "trigger [pattern]    - Send response on pattern",
        "spam [text] [ms]     - Write text every ms",
        "xmodem <send> <path> - Send file via XMODEM",
        "xmodem <recv> <path> - Receive file via XMODEM",
        "config               - Configure settings",
        "swap                 - Swap RX and TX GPIOs",
        "['Hello'] [r:64]...  - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdHdUart() {
    printHeader("HDUART");
    static const char* const lines[] = {
        "bridge               - Half-duplex I/O",
        "config               - Configure settings",
        "[0x1 D:10 r:255]     - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdI2c() {
    printHeader("I2C");
    static const char* const lines[] = {
        "scan                 - Find devices",
        "discovery            - Report on devices",
        "ping <addr>          - Check ACK",
        "identify <addr>      - Identify device",
        "sniff                - View traffic",
        "slave <addr>         - Emulate I2C device",
        "read <addr> [reg]    - Read register",
        "write <a> [r] [val]  - Write register",
        "dump <addr> [len]    - Read all registers",
        "regs <addr> [len]    - Probe register R/W",
        "glitch <addr>        - Run attack sequence",
        "flood <addr>         - Saturate target I/O",
        "health <addr>        - Perform timing test",
        "monitor <addr> [ms]  - Monitor register changes",
        "trace <a> [reg] [ms] - Monitor one register",
        "eeprom [addr]        - I2C EEPROM operations",
        "recover              - Attempt bus recovery",
        "jam                  - Jam I2C bus with noise",
        "swap                 - Swap SDA and SCL GPIOs",
        "config               - Configure settings",
        "[0x13 0x4B 0x1]      - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdSpi() {
    printHeader("SPI");
    static const char* const lines[] = {
        "sniff                - View traffic",
        "sdcard               - SD operations",
        "slave                - Emulate SPI slave",
        "flash                - SPI Flash operations",
        "eeprom               - SPI EEPROM operations",
        "config               - Configure settings",
        "[0x9F r:3]           - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdTwoWire() {
    printHeader("2WIRE");
    static const char* const lines[] = {
        "sniff                - View 2WIRE traffic",
        "smartcard            - Smartcard operations",
        "config               - Configure settings",
        "[0xAB r:4]           - Instruction syntax"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdThreeWire() {
    printHeader("3WIRE");
    static const char* const lines[] = {
        "eeprom               - 3WIRE EEPROM operations",
        "config               - Configure settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdDio() {
    printHeader("DIO");
    static const char* const lines[] = {
        "scan                  - Detect pins activity",
        "pins                  - Show pins state",
        "sniff <gpio>          - Track toggle states",
        "read <gpio>           - Get pin state",
        "set <gpio> <H/L/I/O>  - Set pin state",
        "pullup <gpio>         - Set pin pullup",
        "pulldown <gpio>       - Set pin pulldown",
        "pulse <gpio> <us>     - Send pulse on pin",
        "servo <gpio> <angle>  - Set servo angle",
        "pwm <gpio> [frq duty%]- Set PWM on pin",
        "toggle <gpio> <ms>    - Toggle pin periodically",
        "measure <gpio> [ms]   - Calculate frequency",
        "jam <gpio> [min max]  - Random high/low states",
        "reset <gpio>          - Reset to default"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdLed() {
    printHeader("LED");
    static const char* const lines[] = {
        // "scan                 - Try to detect LEDs type",
        "fill <color>         - Fill all LEDs with a color",
        "set <index> <color>  - Set specific LED color",
        "blink                - Blink all LEDs",
        "rainbow              - Rainbow animation",
        "chase                - Chasing light effect",
        "cycle                - Cycle through colors",
        "wave                 - Wave animation",
        "reset                - Turn off all LEDs",
        "setprotocol          - Select LED protocol",
        "config               - Configure LED settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdInfrared() {
    printHeader("INFRARED");
    static const char* const lines[] = {
        "send <dev> sub <cmd> - Send IR signal",
        "receive              - Receive IR signal",
        "setprotocol          - Set IR protocol type",
        "devicebgone          - OFF devices blast",
        "remote               - Universal remote commands",
        "replay [count]       - Replay recorded IR frames",
        "record               - Record IR signals to file",
        "load                 - Load .ir files from FS",
        "jam                  - Send random IR signals",
        "config               - Configure settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdUsb() {
    printHeader("USB");
    static const char* const lines[] = {
        "storage              - Mount SD as USB",
        "keyboard [text]      - Start keyboard bridge",
        "mouse [action]       - Mouse move and click",
        "mouse jiggle [ms]    - Random mouse moves",
        "gamepad [key]        - Gamepad button press",
        "sysctrl [action]     - Hardware control actions",
        "host                 - Connect device to ESP32",
        "adapters             - Dedicated USB adapters",
        "reset                - Reset interface",
        "config               - Configure settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdBluetooth() {
    printHeader("BLUETOOTH");
    static const char* const lines[] = {
        "scan                 - Discover devices",
        "pair <mac>           - Pair with a device",
        "sniff                - Sniff Bluetooth data",
        "spoof <mac>          - Spoof mac address",
        "status               - Show current status",
        "server               - Create an HID server",
        "keyboard [text]      - Start keyboard bridge",
        "mouse [action]       - Move and click mouse",
        "mouse jiggle [ms]    - Random mouse moves",
        "reset                - Reset interface"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdWifi() {
    printHeader("WIFI");
    static const char* const lines[] = {
        "scan                 - List Wi-Fi networks",
        "connect              - Connect to a network",
        "ping <host>          - Ping a remote host",
        "discovery [timeout]  - Discover network devices",
        "sniff                - Monitor Wi-Fi packets",
        "waterfall            - Show channel activity",
        "probe                - Search for net access",
        "repeater             - Forward Wi-Fi traffic",
        "spoof ap <mac>       - Spoof AP MAC",
        "spoof sta <mac>      - Spoof Station MAC",
        "status               - Show Wi-Fi status",
        "hostname <name>      - Set Wi-Fi hostname",
        "deauth [ssid]        - Deauthenticate hosts",
        "disconnect           - Disconnect from Wi-Fi",
        "ap <ssid> <password> - Set access point",
        "spam                 - Spam random beacons",
        "flood [channel]      - Flood channel with packets",
        "ssh [h] [u] [pw] [p] - Open SSH session",
        "telnet <host> [port] - Open telnet session",
        "nc <host> <port>     - Open netcat session",
        "nmap <h> [-p ports]  - Scan host ports",
        "modbus <host> [port] - Modbus TCP operations",
        "http get <url>       - HTTP(s) GET request",
        "http analyze <url>   - Get analysis report",
        "lookup mac|ip <addr> - Lookup MAC or IP address",
        "webui                - Show the web UI IP",
        "reset                - Reset interface"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdJtag() {
    printHeader("JTAG");
    static const char* const lines[] = {
        "scan swd             - Scan SWD pins",
        "scan jtag            - Scan JTAG pins",
        "openocd              - Reboot into OpenOCD adapter",
        "config               - Configure settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdI2s() {
    printHeader("I2S");
    static const char* const lines[] = {
        "play <freq> [ms]     - Play sine wave for ms",
        "record               - Read mic continuously",
        "test <speaker|mic>   - Run basic audio tests",
        "reset                - Reset to default",
        "config               - Configure settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdCan() {
    printHeader("CAN (MCP2515)");
    static const char* const lines[] = {
        "sniff                - Print all received frames",
        "send [id]            - Send frame with given ID",
        "receive [id]         - Capture frames with ID",
        "status               - State of the CAN controller",
        "config               - Configure MCP2515 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdEthernet() {
    printHeader("ETHERNET (W5500)");
    static const char* const lines[] = {
        "connect              - Connect using DHCP",
        "status               - Show ETH status",
        "ping <host>          - Ping a remote host",
        "discovery [timeout]  - Discover network devices",
        "ssh [h] [u] [pw] [p] - Open SSH session",
        "telnet <host> [port] - Open telnet session",
        "nc <host> <port>     - Open netcat session",
        "nmap <h> [-p ports]  - Scan host ports",
        "modbus <host> [port] - Modbus TCP operations",
        "http get <url>       - HTTP(s) GET request",
        "http analyze <url>   - Get analysis report",
        "lookup mac|ip <addr> - Lookup MAC or IP address",
        "reset                - Reset interface",
        "config               - Configure W5500 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdSubGhz() {
    printHeader("SUBGHZ (CC1101)");
    static const char* const lines[] = {
        "scan                 - Search best frequency",
        "sweep                - Analyze frequency band",
        "send <payload> [te]  - Send a frame payload",
        "receive              - Receive raw/decoded signals",
        "replay               - Record and replay frames",
        "jam                  - Jam selected frequencies",
        "bruteforce           - Bruteforce 12 bit keys",
        "trace                - Observe RX signal trace",
        "waterfall            - Show frequency peaks",
        "record               - Record frame to .sub file",
        "load                 - Load .sub files from FS",
        "ear                  - RSSI to audio mapping",
        "setfrequency         - Set operating frequency",
        "config               - Configure CC1101 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdRfid() {
    printHeader("RFID (PN532)");
    static const char* const lines[] = {
        "read                 - Read RFID tag data",
        "write                - Write UID/Block to tag",
        "clone                - Clone Mifare UID",
        "erase                - Erase RFID tag",
        "config               - Configure PN532 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdRf24() {
    printHeader("RF24 (NRF24L01)");
    static const char* const lines[] = {
        "scan                 - Search best active channel",
        "send                 - Send a frame payload",
        "receive              - Receive frames",
        "sweep                - Analyze channels activity",
        "jam                  - Jam selected channels group",
        "waterfall            - Show channel peaks",
        "setchannel           - Set operating channel",
        "config               - Configure NRF24 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdFm() {
    printHeader("FM (SI4713)");
    static const char* const lines[] = {
        "sweep                - Analyze frequencies",
        "trace [freq]         - Observe frequency signal",
        "waterfall            - Show frequency peaks",
        "broadcast            - Create FM station",
        "reset                - Reset via pin", 
        "config               - Configure Si4713 settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdCell() {
    printHeader("CELL (SIMCOM)");
    static const char* const lines[] = {
        "modem                - Show modem info",
        "network              - Show network info",
        "operator             - Show available operators",
        "sim                  - Show SIM card info",
        "unlock               - Unlock SIM with PIN",
        "phonebook            - Show SIM phonebook",
        "sms                  - SMS operations",
        "call                 - Call operations",
        "ussd [code]          - Send USSD code",
        "setmode              - Set modem mode",
        "config               - Configure modem settings"
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdExpander() {
    printHeader("EXPANDER");

    static const char* const lines[] = {
        "This mode bridges to a expander over UART.",
        "The expander acts as a radio expansion module.",
        "https://github.com/geo-tp/ESP32-Bus-Expander",
    };

    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdLoRa() {
    printHeader("LORA (SX1262)");
    static const char* const lines[] = {
        "send [payload]      - Send text or hex{ AA }",
        "spam [payload] [ms] - Repeat packet every ms",
        "jam [seconds]       - Lab CW for 1..60 seconds",
        "receive             - Listen for packets",
        "record              - Receive and save packet",
        "load                - Send saved .lora packet",
        "rssi [ms]           - Monitor RSSI On channel",
        "ear                 - Map RSSI to audio",
        "scan                - Scan and find best frequency",
        "waterfall           - Show frequency activity",
        "cad [ms]            - Monitor activity on channel",
        "airtime [bytes]     - Airtime for 1..255 bytes",
        "setfreq [MHz]       - Set operating frequency",
        "status              - Show profile and counters",
        "meshtastic          - Open Meshtastic shell",
        "config              - Configure LoRa SX1262",
    };
    printLines(lines, (int)(sizeof(lines) / sizeof(lines[0])));
}

void HelpShell::cmdAll() {
    cmdOneWire();
    cmdUart();
    cmdHdUart();
    cmdI2c();
    cmdSpi();
    cmdTwoWire();
    cmdThreeWire();
    cmdDio();
    cmdLed();
    cmdInfrared();
    cmdUsb();
    cmdBluetooth();
    cmdWifi();
    cmdJtag();
    cmdI2s();
    cmdCan();
    cmdEthernet();
    cmdSubGhz();
    cmdRfid();
    cmdRf24();
    cmdFm();
    cmdCell();
    cmdLoRa();
    cmdExpander();
}
