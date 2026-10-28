#include "UsbAdapterShell.h"
#include <algorithm>
#include <vector>

UsbAdapterShell::UsbAdapterShell(ITerminalView& tv,
                                 IInput& in,
                                 IUtilityService& utilityService,
                                 UserInputManager& uim,
                                 NvsService& nvs,
                                 SystemService& systemService)
    : terminalView(tv),
      terminalInput(in),
      utilityService(utilityService),
      userInputManager(uim),
      nvsService(nvs),
      systemService(systemService) {}

void UsbAdapterShell::run() {
    terminalView.println("\n=== USB Adapters ===");
    terminalView.println("Adapters reboot into a dedicated USB mode.");
    terminalView.println("The next reset will return to normal mode.");
    terminalView.println("https://github.com/geo-tp/ESP32-Bit-Pirate/wiki/99-Adapters\n");

    int choice = userInputManager.readValidatedChoiceIndex("Select adapter", actions, actionsCount, actionsCount - 1);
    if (choice == 0) {
        rebootUsbUartBridge();
        return;
    }
    if (choice == 1) {
        rebootFlashromSerprog();
        return;
    }
    if (choice == 2) {
        rebootAvrDudeBusPirate();
        return;
    }
    if (choice == 3) {
        rebootSumpLogicAnalyzer();
        return;
    }
    if (choice == 4) {
        rebootOpenOcdBusPirate();
        return;
    }
    if (choice == 5) {
        rebootInfraredToy();
        return;
    }
    if (choice == 6) {
        rebootSubGhzRawCdc();
        return;
    }
    if (choice == 7) {
        rebootBpio2();
        return;
    }
    if (choice == 8) {
        rebootSdrCdc();
        return;
    }
    
    terminalView.println("Exiting USB adapters...\n");
}

void UsbAdapterShell::rebootIntoAdapter(const char* title,
                                        const char* description,
                                        const char* example,
                                        const char* webTool,
                                        const char* returnInstruction) {
    terminalView.println(std::string("Rebooting into ") + title + " mode.");
    terminalView.println(description);
    terminalView.println(example);
    if (webTool) {
        terminalView.println(std::string("Or use the web tool:\n\rhttps://geo-tp.github.io/ESP32-Bit-Pirate/web-tools/") + webTool + "/");
    }
    terminalView.println(returnInstruction);
    terminalView.println("The terminal will now close...");
    utilityService.sleepMs(1000);
    systemService.reboot();
}

void UsbAdapterShell::rebootUsbUartBridge() {
    auto forbidden = state.getProtectedPins();

    terminalView.println("\nUSB-UART adapter GPIOs:");
    terminalView.println("Adapter RX is ESP input; connect it to target TX.");
    terminalView.println("Adapter TX is ESP output; connect it to target RX.");
    uint8_t rxPin = userInputManager.readValidatedPinNumber("Adapter RX GPIO (connect target TX)", state.getUartRxPin(), forbidden);
    forbidden.push_back(rxPin);

    uint8_t txPin = userInputManager.readValidatedPinNumber("Adapter TX GPIO (connect target RX)", state.getUartTxPin(), forbidden);
    bool inverted = state.isUartInverted();

    nvsService.open();
    nvsService.saveOneShotUsbUartBridgeConfig(rxPin, txPin, inverted);
    nvsService.saveOneShotBootMode(OneShotBootMode::UsbUartBridge);
    nvsService.close();

    rebootIntoAdapter(
        "USB-UART adapter",
        "The device will expose one CDC serial port as the UART bridge.",
        "Example: picocom -b 115200 /dev/ttyACM0",
        "web-serial-terminal",
        "Reset the device to return to normal mode."
    );
}

void UsbAdapterShell::rebootSumpLogicAnalyzer() {
    auto forbidden = state.getProtectedPins();
    std::vector<uint8_t> defaultPins = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMISOPin(),
        state.getSpiMOSIPin()
    };

    terminalView.println("\nSUMP logic analyzer GPIOs:");
    terminalView.println("This mode exposes a PulseView/sigrok OLS-compatible SUMP device.");
    terminalView.println("Select up to 8 GPIOs. Pin order maps to channels D0..D7.");
    terminalView.println("Sample rate and capture size are configured by PulseView/sigrok.\n");

    std::vector<uint8_t> selectedPins = userInputManager.readValidatedPinGroup(
        "Logic GPIOs D0..D7",
        defaultPins,
        forbidden
    );

    if (selectedPins.size() > 8) {
        selectedPins.resize(8);
        terminalView.println("Using first 8 GPIOs only.");
    }

    uint8_t pins[8] = {};
    for (size_t i = 0; i < selectedPins.size(); ++i) {
        pins[i] = selectedPins[i];
    }

    nvsService.open();
    nvsService.saveOneShotSumpLogicAnalyzerConfig(
        pins,
        static_cast<uint8_t>(selectedPins.size())
    );
    nvsService.saveOneShotBootMode(OneShotBootMode::SumpLogicAnalyzer);
    nvsService.close();

    rebootIntoAdapter(
        "SUMP logic analyzer",
        "The device will expose one CDC serial port for PulseView/sigrok.",
        "Open PulseView, Select Driver Logic Sniffer & SUMP, and Serial Port",
        "logic-analyzer"
    );
}

void UsbAdapterShell::rebootOpenOcdBusPirate() {
    auto forbiddenJtag = state.getProtectedPins();

    terminalView.println("\nOpenOCD Bus Pirate adapter GPIOs:");
    terminalView.println("This mode exposes a Bus Pirate compatible OpenOCD transport.");
    terminalView.println("OpenOCD may select JTAG or SWD at connection time.");
    terminalView.println("JTAG uses TCK/TMS/TDI/TDO. SWD uses SWCLK/SWDIO.\n");

    uint8_t tckPin = userInputManager.readValidatedPinNumber("JTAG TCK GPIO", state.getSpiCLKPin(), forbiddenJtag);
    forbiddenJtag.push_back(tckPin);

    uint8_t tmsPin = userInputManager.readValidatedPinNumber("JTAG TMS GPIO", state.getSpiCSPin(), forbiddenJtag);
    forbiddenJtag.push_back(tmsPin);

    uint8_t tdiPin = userInputManager.readValidatedPinNumber("JTAG TDI GPIO", state.getSpiMOSIPin(), forbiddenJtag);
    forbiddenJtag.push_back(tdiPin);

    uint8_t tdoPin = userInputManager.readValidatedPinNumber("JTAG TDO GPIO", state.getSpiMISOPin(), forbiddenJtag);

    auto forbiddenSwd = state.getProtectedPins();
    uint8_t swclkPin = userInputManager.readValidatedPinNumber("SWD SWCLK GPIO", tckPin, forbiddenSwd);
    forbiddenSwd.push_back(swclkPin);

    uint8_t swdioPin = userInputManager.readValidatedPinNumber("SWD SWDIO GPIO", tmsPin, forbiddenSwd);

    nvsService.open();
    nvsService.saveOneShotOpenOcdBusPirateConfig(tckPin, tmsPin, tdiPin, tdoPin, swclkPin, swdioPin);
    nvsService.saveOneShotBootMode(OneShotBootMode::OpenOcdBusPirate);
    nvsService.close();

    rebootIntoAdapter(
        "OpenOCD Bus Pirate adapter",
        "Use OpenOCD with interface/buspirate.cfg on the new CDC serial port.",
        "Example SWD: openocd -f interface/buspirate.cfg -c \"buspirate port /dev/ttyACM0; transport select swd\" -f target/stm32f1x.cfg"
    );
}

void UsbAdapterShell::rebootAvrDudeBusPirate() {
    auto forbidden = state.getProtectedPins();

    terminalView.println("\nAVRDUDE Bus Pirate SPI adapter GPIOs:");
    terminalView.println("This mode exposes the Bus Pirate legacy binary SPI protocol.");
    terminalView.println("CS is used as AVR RESET by avrdude -c buspirate.");
    terminalView.println("Target power is not supplied by this adapter; power the AVR separately.\n");

    uint8_t csPin = userInputManager.readValidatedPinNumber("RESET/CS GPIO (connect AVR RESET)", state.getSpiCSPin(), forbidden);
    forbidden.push_back(csPin);

    uint8_t sckPin = userInputManager.readValidatedPinNumber("SCK GPIO (connect AVR SCK)", state.getSpiCLKPin(), forbidden);
    forbidden.push_back(sckPin);

    uint8_t misoPin = userInputManager.readValidatedPinNumber("MISO GPIO (connect AVR MISO)", state.getSpiMISOPin(), forbidden);
    forbidden.push_back(misoPin);

    uint8_t mosiPin = userInputManager.readValidatedPinNumber("MOSI GPIO (connect AVR MOSI)", state.getSpiMOSIPin(), forbidden);

    nvsService.open();
    nvsService.saveOneShotAvrDudeBusPirateConfig(csPin, sckPin, misoPin, mosiPin, 1000000);
    nvsService.saveOneShotBootMode(OneShotBootMode::AvrDudeBusPirate);
    nvsService.close();

    rebootIntoAdapter(
        "AVRDUDE Bus Pirate SPI adapter",
        "Use avrdude on the new CDC serial port.",
        "Example: avrdude -c buspirate -P /dev/ttyACM0 -p m328p -v -x spifreq=1",
        "avr-programmer"
    );
}

void UsbAdapterShell::rebootBpio2() {
    const auto forbidden = state.getProtectedPins();
    const uint8_t candidates[] = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMOSIPin(),
        state.getSpiMISOPin(),
        state.getI2cSclPin(),
        state.getI2cSdaPin(),
        state.getUartTxPin(),
        state.getUartRxPin(),
        state.getOneWirePin(),
        state.getLedDataPin()
    };

    std::vector<uint8_t> defaultPins;
    defaultPins.reserve(8);
    for (uint8_t pin : candidates) {
        if (defaultPins.size() >= 8) break;
        if (std::find(forbidden.begin(), forbidden.end(), pin) != forbidden.end()) continue;
        if (std::find(defaultPins.begin(), defaultPins.end(), pin) != defaultPins.end()) continue;
        defaultPins.push_back(pin);
    }
    for (uint8_t pin = 0; pin <= 48 && defaultPins.size() < 8; ++pin) {
        if (std::find(forbidden.begin(), forbidden.end(), pin) != forbidden.end()) continue;
        if (std::find(defaultPins.begin(), defaultPins.end(), pin) != defaultPins.end()) continue;
        defaultPins.push_back(pin);
    }

    terminalView.println("\nBPIO2 GPIO / SPI / I2C adapter pins:");
    terminalView.println("Select 8 unique GPIOs mapped to BPIO2 IO0..IO7.\n");

    terminalView.println("SPI mapping:");
    terminalView.println("  IO0 = CS");
    terminalView.println("  IO1 = SCK / CLK");
    terminalView.println("  IO2 = MOSI");
    terminalView.println("  IO3 = MISO");
    terminalView.println("  IO4..IO7 = auxiliary GPIOs\n");

    terminalView.println("I2C mapping:");
    terminalView.println("  IO1 = SCL");
    terminalView.println("  IO2 = SDA");
    terminalView.println("  IO0, IO3..IO7 = auxiliary GPIOs\n");

    terminalView.println("The adapter uses the BPIO2 FlatBuffers.");
    terminalView.println("Scripts can switch between HiZ, SPI and I2C.\n");

    std::vector<uint8_t> selectedPins;
    while (true) {
        selectedPins = userInputManager.readValidatedPinGroup(
            "BPIO2 GPIOs IO0..IO7",
            defaultPins,
            forbidden
        );

        bool duplicates = false;
        for (size_t i = 0; i < selectedPins.size() && !duplicates; ++i) {
            for (size_t j = i + 1; j < selectedPins.size(); ++j) {
                if (selectedPins[i] == selectedPins[j]) {
                    duplicates = true;
                    break;
                }
            }
        }

        if (selectedPins.size() == 8 && !duplicates) break;
        terminalView.println("Please select exactly 8 unique GPIOs.");
    }

    nvsService.open();
    nvsService.saveOneShotBpio2Config(selectedPins.data(), 8);
    nvsService.saveOneShotBootMode(OneShotBootMode::Bpio2);
    nvsService.close();

    rebootIntoAdapter(
        "BPIO2 adapter",
        "The device will expose GPIO, hardware SPI and hardware I2C.",
        "Use the Bus Pirate BPIO2 Python client on the serial port.",
        "bpio2"
    );
}

void UsbAdapterShell::rebootFlashromSerprog() {
    auto forbidden = state.getProtectedPins();

    terminalView.println("\nFlashrom SPI adapter GPIOs:");
    terminalView.println("This mode exposes a flashrom serprog SPI programmer.");
    terminalView.println("Use 3.3V flash chips only, or add proper level shifting.");
    terminalView.println("Configured WP# and HOLD# GPIOs are held HIGH while the adapter drives the bus.\n");

    uint8_t csPin = userInputManager.readValidatedPinNumber("Flash CS GPIO (connect chip CS#)", state.getSpiCSPin(), forbidden);
    forbidden.push_back(csPin);

    uint8_t sckPin = userInputManager.readValidatedPinNumber("Flash SCK GPIO (connect chip CLK)", state.getSpiCLKPin(), forbidden);
    forbidden.push_back(sckPin);

    uint8_t misoPin = userInputManager.readValidatedPinNumber("Flash MISO GPIO (connect chip DO/IO1)", state.getSpiMISOPin(), forbidden);
    forbidden.push_back(misoPin);

    uint8_t mosiPin = userInputManager.readValidatedPinNumber("Flash MOSI GPIO (connect chip DI/IO0)", state.getSpiMOSIPin(), forbidden);
    forbidden.push_back(mosiPin);

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
    int8_t wpPin = -1;
    int8_t holdPin = -1;
    if (userInputManager.readYesNo("Use WP or HOLD ?", false)) {
        wpPin = selectControlPin("WP");
        holdPin = selectControlPin("HOLD");
    }

    nvsService.open();
    nvsService.saveOneShotFlashromSerprogConfig(csPin, sckPin, misoPin, mosiPin, state.getSpiFrequency(), wpPin, holdPin);
    nvsService.saveOneShotBootMode(OneShotBootMode::FlashromSerprog);
    nvsService.close();

    rebootIntoAdapter(
        "Flashrom SPI adapter",
        "Use flashrom with serprog on the new CDC serial port.",
        "Example: flashrom -p serprog:dev=/dev/ttyACM0:921600,spispeed=4M",
        "spi-flash-programmer"
    );
}

void UsbAdapterShell::rebootInfraredToy() {
    auto forbidden = state.getProtectedPins();

    terminalView.println("\nUSB IR Toy / LIRC adapter GPIOs:");
    terminalView.println("This mode exposes IR TX/RX as an USB IR compatible CDC adapter.");
    terminalView.println("Use it with LIRC irtoy, xmode2/mode2, or compatible IR tools.\n");

    uint8_t txPin = userInputManager.readValidatedPinNumber("IR TX GPIO", state.getInfraredTxPin(), forbidden);
    forbidden.push_back(txPin);

    uint8_t rxPin = userInputManager.readValidatedPinNumber("IR RX GPIO", state.getInfraredRxPin(), forbidden);

    nvsService.open();
    nvsService.saveOneShotInfraredToyConfig(txPin, rxPin);
    nvsService.saveOneShotBootMode(OneShotBootMode::InfraredToy);
    nvsService.close();

    rebootIntoAdapter(
        "USB IR Toy / LIRC adapter",
        "The device will expose one CDC serial port for LIRC's irtoy driver.",
        "Example: mode2 --driver=irtoy --device=/dev/ttyACM0"
    );
}

void UsbAdapterShell::rebootSdrCdc() {
    nvsService.open();
    nvsService.saveOneShotBootMode(OneShotBootMode::SdrCdc);
    nvsService.close();
    rebootIntoAdapter(
        "SDR Raw RF CDC",
        "Experimental 2.2-2.8 GHz SDR",
        "Use raw CDC at baud 115200 for scripting.",
        "esp-sdr",
        "Reset or press a device button to return to normal mode."
    );
}

void UsbAdapterShell::rebootSubGhzRawCdc() {
    auto forbidden = state.getProtectedPins();

    terminalView.println("\nSubGHz Raw CDC CC1101 adapter GPIOs:");
    terminalView.println("Exposes CC1101 RAW OOK over a CDC serial adapter.");
    terminalView.println("Use with terminal tools or serial scripts.\n");

    uint8_t sckPin = userInputManager.readValidatedPinNumber("CC1101 SCK GPIO", state.getSubGhzSckPin(), forbidden);
    forbidden.push_back(sckPin);

    uint8_t misoPin = userInputManager.readValidatedPinNumber("CC1101 MISO GPIO", state.getSubGhzMisoPin(), forbidden);
    forbidden.push_back(misoPin);

    uint8_t mosiPin = userInputManager.readValidatedPinNumber("CC1101 MOSI GPIO", state.getSubGhzMosiPin(), forbidden);
    forbidden.push_back(mosiPin);

    uint8_t csPin = userInputManager.readValidatedPinNumber("CC1101 SS/CS GPIO", state.getSubGhzCsPin(), forbidden);
    forbidden.push_back(csPin);

    uint8_t gdo0Pin = userInputManager.readValidatedPinNumber("CC1101 GDO0 GPIO", state.getSubGhzGdoPin(), forbidden);
    forbidden.push_back(gdo0Pin);

    float frequencyMhz = userInputManager.readValidatedFloat("Frequency MHz", state.getSubGhzFrequency(), 0.0f, 1000.0f);
    int paDbm = userInputManager.readValidatedInt("TX power dBm", 10, -30, 12);
    uint32_t baudrate = static_cast<uint32_t>(userInputManager.readValidatedInt("CDC baudrate", 38400, 1200, 921600));

    nvsService.open();
    nvsService.saveOneShotSubGhzRawCdcConfig(
        sckPin,
        misoPin,
        mosiPin,
        csPin,
        gdo0Pin,
        frequencyMhz,
        static_cast<int8_t>(paDbm),
        baudrate
    );
    nvsService.saveOneShotBootMode(OneShotBootMode::SubGhzRawCdc);
    nvsService.close();

    rebootIntoAdapter(
        "SubGHz Raw CDC CC1101 adapter",
        "The device will expose one CDC serial port with raw SubGHz ASCII commands.",
        "Example: screen /dev/ttyACM0 38400"
    );
}
