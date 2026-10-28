#include "Configurators/TerminalTypeConfigurator.h"

TerminalTypeConfigurator::TerminalTypeConfigurator(HorizontalSelector& selector)
    : selector(selector) {}

TerminalTypeEnum TerminalTypeConfigurator::configure() {
    std::vector<std::string> options = {
        TerminalTypeEnumMapper::toString(TerminalTypeEnum::WiFiClient),
        TerminalTypeEnumMapper::toString(TerminalTypeEnum::WiFiAp),
        TerminalTypeEnumMapper::toString(TerminalTypeEnum::SerialPort),
        #ifdef DEVICE_CARDPUTER
            TerminalTypeEnumMapper::toString(TerminalTypeEnum::Standalone),
        #endif
    };

    int selected = 2; // Serial

    #if defined(DEVICE_M5STAMPS3) || defined(DEVICE_S3DEVKIT) || defined(DEVICE_UMPROS3) || defined(DEVICE_CUSTOM)
        selected = selector.selectHeadless();
    #else
        selected = selector.select(
            "ESP32 BIT PIRATE",
            options,
            "Select terminal type",
            ""
        );
    #endif

    switch (selected) {
        case 0: return TerminalTypeEnum::WiFiClient;
        case 1: return TerminalTypeEnum::WiFiAp;
        case 2: return TerminalTypeEnum::SerialPort;
        case 3: return TerminalTypeEnum::Standalone;
        default: return TerminalTypeEnum::None;
    }
}
