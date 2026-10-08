#pragma once
#include "Models/LoRaRadioProfile.h"

#include <cstdint>
#include <string>
#include <sstream>
#include "Enums/InfraredProtocolEnum.h"
#include "Enums/ModeEnum.h"
#include "Enums/TerminalTypeEnum.h"
#include <array>

class GlobalState {
private:
    // Version
    static constexpr const char* version = "1.7";

    //Pin in use
    std::vector<uint8_t> protectedPins;

    // Builtin
    #ifdef LED_PIN
        static constexpr uint8_t ledPin = LED_PIN;
    #else
        static constexpr uint8_t ledPin = 21;
    #endif

    // SPI
    uint8_t spiCSPin = 12;
    uint8_t spiCLKPin = 40;
    uint8_t spiMISOPin = 39;
    uint8_t spiMOSIPin = 14;
    int8_t spiWPPin = -1;
    int8_t spiHOLDPin = -1;
    uint32_t spiFrequency = 20000000;

    // WiFi AP Credentials
    static constexpr const char* apName = "ESP32-Bit-Pirate";
    static constexpr const char* apPassword = "readytoboard";

    // NVS
    static constexpr const char* nvsSsidField = "ssid";
    static constexpr const char* nvsPasswordField = "pass";
    static constexpr const char* nvsHostnameField = "hostname";

    // Terminal Web UI
    std::string terminalIp = "0.0.0.0";
    std::string activeApName = apName;

    // Terminal transmission mode
    TerminalTypeEnum terminalMode = TerminalTypeEnum::SerialPort;

    //  Current selected mode
    ModeEnum currentMode = ModeEnum::HIZ;

    // PC Terminal Serial Configuration
    static constexpr unsigned long serialTerminalBaudRate = 115200;

    // OneWire Default Pin
    uint8_t oneWirePin = 1; // pin par défaut

    // TwoWire Default Configuration
    uint8_t twoWireClkPin = 1;
    uint8_t twoWireIoPin = 2;
    uint8_t twoWireRstPin = 3;

    // ThreeWire Default Pins
    uint8_t threeWireCsPin = 5;
    uint8_t threeWireSkPin = 18;
    uint8_t threeWireDiPin = 23;
    uint8_t threeWireDoPin = 19;
    bool threeWireOrg8 = false; // 16-bit organization by default
    uint8_t threeWireeEepromModelIndex = 0; // Default eeprom model index

    // UART Default Configuration
    unsigned long uartBaudRate = 9600;
    uint32_t uartConfig = 0x800001c; // SERIAL_8N1 by default
    bool uartInverted = false;
    uint8_t uartRxPin = 1;
    uint8_t uartTxPin = 2;
    uint8_t uartDataBits = 8;
    std::string uartParity = "None";
    bool uartFlowControl = false;
    uint8_t uartStopBits = 1;

    // HDUART Default Configuration
    unsigned long hdUartBaudRate = 9600;
    uint32_t hdUartConfig = 0x800001c; // SERIAL_8N1
    bool hdUartInverted = false;
    uint8_t hdUartPin = 1;
    uint8_t hdUartDataBits = 8;
    std::string hdUartParity = "N";
    bool hdUartFlowControl = false;
    uint8_t hdUartStopBits = 1;

    // I2C Default Configuration
    uint8_t i2cSclPin = 1;
    uint8_t i2cSdaPin = 2;
    uint32_t i2cFrequency = 100000;

    // Infrared Default Configuration
    uint8_t infraredPin = 44; 
    uint8_t infraredRxPin = 1;
    InfraredProtocolEnum infraredProtocol = InfraredProtocolEnum::_NEC;

    // LED Mode
    uint8_t ledDataPin = 1;
    uint8_t ledClockPin = 2;
    uint16_t ledLength = 1;
    std::string ledProtocol = "ws2812";
    uint8_t ledBrightness = 128;

    // I2S Default Configuration
    uint8_t i2sBclkPin = 41;
    uint8_t i2sLrckPin = 43;
    uint8_t i2sDataPin = 42;
    uint32_t i2sSampleRate = 44100;
    uint8_t i2sBitsPerSample = 16;
    uint8_t i2sPercentLevel = 100;

    // CAN Default Configuration
    uint8_t canCspin = 1;
    uint8_t canSckPin = 0;
    uint8_t canSiPin = 2;
    uint8_t canSoPin = 3;
    uint32_t canKbps = 120;

    // Ethernet Default Configuration
    uint8_t ethernetCsPin = 5;
    uint8_t ethernetRstPin = 4;
    uint8_t ethernetSckPin = 18;
    uint8_t ethernetMisoPin = 19;
    uint8_t ethernetMosiPin = 23;
    uint8_t ethernetIrqPin = 39;
    uint32_t ethernetFrequency = 20000000; // 20 MHz
    std::array<uint8_t,6> ethernetMac = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42 };

    // SubGHz Default Configuration
    uint8_t subGhzSckPin = 12;
    uint8_t subGhzMisoPin = 13;
    uint8_t subGhzMosiPin = 14;
    uint8_t subGhzCsPin = 15;
    uint8_t subGhzGdoPin = 27;
    float subGhzFrequency = 433.92; // mhz
    uint8_t subGhzPower = 10; // dBm
    uint8_t subGhzModulation = 2; // 0=FSK, 1=OOK, 2=ASK
    uint32_t subGhzBandwidth = 125000; // 125 kHz

    // NRF24 Default Configuration
    uint8_t rf24CsnPin = 1;
    uint8_t rf24CePin = 3;
    uint8_t rf24SckPin = 5;
    uint8_t rf24MisoPin = 7;
    uint8_t rf24MosiPin = 9;

    // LoRa SX1262 Default Configuration
    uint8_t loRaSckPin = 5, loRaMisoPin = 6, loRaMosiPin = 7, loRaCsPin = 4;
    uint8_t loRaRstPin = 3, loRaBusyPin = 2, loRaDio1Pin = 1;
    float loRaFrequency = 868.0f;
    float loRaTcxoVoltage = 1.8f;
    uint16_t loRaBandwidth = 125;
    uint8_t loRaSpreadingFactor = 9;
    uint8_t loRaCodingRate = 7;
    int8_t loRaPower = 14;
    uint16_t loRaPreambleLength = 8;
    uint16_t loRaSyncWord = 0x1424;
    bool loRaCrc = true;
    bool loRaInvertIq = false;

    // RFID (I2C PN532)
    uint8_t rfidSdaPin = 1;
    uint8_t rfidSclPin = 2;

    // JTAG Default Pin
    std::vector<uint8_t> jtagScanPins = { 1, 3, 5, 7, 9 };

    // SD Card Default Configuration
    uint8_t sdCardCsPin = 12;
    uint8_t sdCardClkPin = 40;
    uint8_t sdCardMisoPin = 39;
    uint8_t sdCardMosiPin = 14;
    uint32_t sdCardFrequency = 20000000; // 20 MHz
    #ifdef SDCARD_CS_PIN
        static constexpr bool hasInternalSdCard = true;
    #else
        static constexpr bool hasInternalSdCard = false;
    #endif

    // SD Card File Limits
    static constexpr size_t fileCountLimit = 512;
    static constexpr size_t fileCacheLimit = 64;

    // USB Default Configuration (not used atm)
    static constexpr const char* usbProductString = "ESP32-Bit-Pirate";
    static constexpr const char* usbManufacturerString = "Free Open Source";
    static constexpr const char* usbSerialString = "";
    static constexpr uint16_t usbVid = 0x303A;   // VID for USB device descriptor
    static constexpr uint16_t usbPid = 0x1001;   // PID for USB device descriptor
    static constexpr const char* webUSBString = "https://geo-tp.github.io/ESP32-Bit-Pirate/webflasher/";
public:
    GlobalState(const GlobalState&) = delete;
    GlobalState& operator=(const GlobalState&) = delete;

    static GlobalState& getInstance() {
        static GlobalState instance;
        return instance;
    }

    // Version
    const char* getVersion() const { return version; }

    // Builtin
    uint8_t getLedPin() const { return ledPin; }

    // SPI
    uint8_t getSpiCSPin() const { return spiCSPin; }
    uint8_t getSpiCLKPin() const { return spiCLKPin; }
    uint8_t getSpiMISOPin() const { return spiMISOPin; }
    uint8_t getSpiMOSIPin() const { return spiMOSIPin; }
    int8_t getSpiWPPin() const { return spiWPPin; }
    int8_t getSpiHOLDPin() const { return spiHOLDPin; }
    uint32_t getSpiFrequency() const { return spiFrequency; }
    
    void setSpiCSPin(uint8_t pin) { spiCSPin = pin; }
    void setSpiCLKPin(uint8_t pin) { spiCLKPin = pin; }
    void setSpiMISOPin(uint8_t pin) { spiMISOPin = pin; }
    void setSpiMOSIPin(uint8_t pin) { spiMOSIPin = pin; }
    void setSpiWPPin(int8_t pin) { spiWPPin = pin; }
    void setSpiHOLDPin(int8_t pin) { spiHOLDPin = pin; }
    void setSpiFrequency(uint32_t freq) { spiFrequency = freq; }

    // AP WiFi
    const char* getApName() const { return apName; }
    const char* getApPassword() const { return apPassword; }
    const std::string& getActiveApName() const { return activeApName; }
    void setActiveApName(const std::string& ssid) { activeApName = ssid; }

    // Terminal IP
    const std::string& getTerminalIp() const { return terminalIp; }
    void setTerminalIp(const std::string& ip) { terminalIp = ip; }

    // Terminal transmission mode
    TerminalTypeEnum getTerminalMode() const { return terminalMode; }
    void setTerminalMode(TerminalTypeEnum mode) { terminalMode = mode; }

    // Current Mode
    ModeEnum getCurrentMode() const { return currentMode; }
    void setCurrentMode(ModeEnum mode) { currentMode = mode; }

    // Serial Terminal Baud
    unsigned long getSerialTerminalBaudRate() const { return serialTerminalBaudRate; }
    // OneWire
    uint8_t getOneWirePin() const { return oneWirePin; }
    void setOneWirePin(uint8_t pin) { oneWirePin = pin; }

    // TwoWire
    uint8_t getTwoWireClkPin() const { return twoWireClkPin; }
    uint8_t getTwoWireIoPin() const { return twoWireIoPin; }
    uint8_t getTwoWireRstPin() const { return twoWireRstPin; }

    void setTwoWireClkPin(uint8_t pin) { twoWireClkPin = pin; }
    void setTwoWireIoPin(uint8_t pin) { twoWireIoPin = pin; }
    void setTwoWireRstPin(uint8_t pin) { twoWireRstPin = pin; }

    // ThreeWire
    uint8_t getThreeWireCsPin() const { return threeWireCsPin; }
    uint8_t getThreeWireSkPin() const { return threeWireSkPin; }
    uint8_t getThreeWireDiPin() const { return threeWireDiPin; }
    uint8_t getThreeWireDoPin() const { return threeWireDoPin; }
    bool isThreeWireOrg8() const { return threeWireOrg8; }
    uint8_t getThreeWireEepromModelIndex() const { return threeWireeEepromModelIndex; }

    void setThreeWireCsPin(uint8_t pin) { threeWireCsPin = pin; }
    void setThreeWireSkPin(uint8_t pin) { threeWireSkPin = pin ; }
    void setThreeWireDiPin(uint8_t pin) { threeWireDiPin = pin; }
    void setThreeWireDoPin(uint8_t pin) { threeWireDoPin = pin; }
    void setThreeWireOrg8(bool org8) { threeWireOrg8 = org8; }
    void setThreeWireEepromModelIndex(uint8_t index) { threeWireeEepromModelIndex = index; }

    // UART
    unsigned long getUartBaudRate() const { return uartBaudRate; }
    uint32_t getUartConfig() const { return uartConfig; }
    uint8_t getUartRxPin() const { return uartRxPin; }
    uint8_t getUartTxPin() const { return uartTxPin; }
    uint8_t getUartDataBits() const { return uartDataBits; }
    const std::string& getUartParity() const { return uartParity; }
    uint8_t getUartStopBits() const { return uartStopBits; }
    bool isUartInverted() const { return uartInverted; }
    bool isUartFlowControl() const { return uartFlowControl; }

    void setUartBaudRate(unsigned long b) { uartBaudRate = b; }
    void setUartConfig(uint32_t c) { uartConfig = c; }
    void setUartInverted(bool inv) { uartInverted = inv; }
    void setUartRxPin(uint8_t pin) { uartRxPin = pin; }
    void setUartTxPin(uint8_t pin) { uartTxPin = pin; }
    void setUartDataBits(uint8_t bits) { uartDataBits = bits; }
    void setUartParity(const std::string& parityVal) { uartParity = parityVal; }
    void setUartFlowControl(bool enabled) { uartFlowControl = enabled; }
    void setUartStopBits(uint8_t bits) { uartStopBits = bits; }

    // HDUART
    unsigned long getHdUartBaudRate() const { return hdUartBaudRate; }
    uint32_t getHdUartConfig() const { return hdUartConfig; }
    uint8_t getHdUartPin() const { return hdUartPin; }
    uint8_t getHdUartDataBits() const { return hdUartDataBits; }
    const std::string& getHdUartParity() const { return hdUartParity; }
    uint8_t getHdUartStopBits() const { return hdUartStopBits; }
    bool isHdUartInverted() const { return hdUartInverted; }
    bool isHdUartFlowControl() const { return hdUartFlowControl; }

    void setHdUartBaudRate(unsigned long b) { hdUartBaudRate = b; }
    void setHdUartConfig(uint32_t c) { hdUartConfig = c; }
    void setHdUartInverted(bool inv) { hdUartInverted = inv; }
    void setHdUartPin(uint8_t pin) { hdUartPin = pin; }
    void setHdUartDataBits(uint8_t bits) { hdUartDataBits = bits; }
    void setHdUartParity(const std::string& parityVal) { hdUartParity = parityVal; }
    void setHdUartFlowControl(bool enabled) { hdUartFlowControl = enabled; }
    void setHdUartStopBits(uint8_t bits) { hdUartStopBits = bits; }

    // I2C
    uint8_t getI2cSdaPin() const { return i2cSdaPin; }
    uint8_t getI2cSclPin() const { return i2cSclPin; }
    uint32_t getI2cFrequency() const { return i2cFrequency; }

    void setI2cSdaPin(uint8_t pin) { i2cSdaPin = pin; }
    void setI2cSclPin(uint8_t pin) { i2cSclPin = pin; }
    void setI2cFrequency(uint32_t freq) { i2cFrequency = freq; }

    // Infrared
    uint8_t getInfraredTxPin() const { return infraredPin; }
    uint8_t getInfraredRxPin() const { return infraredRxPin; }
    void setInfraredTxPin(uint8_t pin) { infraredPin = pin; }
    void setInfraredRxPin(uint8_t pin) { infraredRxPin = pin; }
    InfraredProtocolEnum getInfraredProtocol() const { return infraredProtocol ; }
    void setInfraredProtocol(InfraredProtocolEnum prot) {  infraredProtocol = prot; }

    // LED Mode
    uint8_t getLedDataPin() const { return ledDataPin; }
    uint8_t getLedClockPin() const { return ledClockPin; }
    uint16_t getLedLength() const { return ledLength; }
    const std::string& getLedProtocol() const { return ledProtocol; }
    uint8_t getLedBrightness() const { return ledBrightness; }

    void setLedDataPin(uint8_t pin) { ledDataPin = pin; }
    void setLedClockPin(uint8_t pin) { ledClockPin = pin; }
    void setLedLength(uint16_t len) { ledLength = len; }
    void setLedProtocol(const std::string& prot) { ledProtocol = prot; }
    void setLedBrightness(uint8_t brightness) { ledBrightness = brightness; }

    // I2S
    uint8_t getI2sBclkPin() const { return i2sBclkPin; }
    uint8_t getI2sLrckPin() const { return i2sLrckPin; }
    uint8_t getI2sDataPin() const { return i2sDataPin; }
    uint32_t getI2sSampleRate() const { return i2sSampleRate; }
    uint8_t getI2sBitsPerSample() const { return i2sBitsPerSample; }
    uint8_t getI2sPercentLevel() const {return i2sPercentLevel;}

    void setI2sBclkPin(uint8_t pin) { i2sBclkPin = pin; }
    void setI2sLrckPin(uint8_t pin) { i2sLrckPin = pin; }
    void setI2sDataPin(uint8_t pin) { i2sDataPin = pin; }
    void setI2sSampleRate(uint32_t rate) { i2sSampleRate = rate; }
    void setI2sBitsPerSample(uint8_t bits) { i2sBitsPerSample = bits; }
    void setI2sPercentLevel(uint8_t level) {i2sPercentLevel = level;}

    // JTAG
    const std::vector<uint8_t>& getJtagScanPins() const { return jtagScanPins; }
    void setJtagScanPins(const std::vector<uint8_t>& pins) { jtagScanPins = pins; }

    // CAN
    uint8_t getCanCspin() const { return canCspin; }
    uint8_t getCanSckPin() const { return canSckPin; }
    uint8_t getCanSiPin() const { return canSiPin; }
    uint8_t getCanSoPin() const { return canSoPin; }
    uint32_t getCanKbps() const { return canKbps; }

    void setCanCspin(uint8_t pin) { canCspin = pin; }
    void setCanSckPin(uint8_t pin) { canSckPin = pin; }
    void setCanSiPin(uint8_t pin) { canSiPin = pin; }
    void setCanSoPin(uint8_t pin) { canSoPin = pin; }
    void setCanKbps(uint32_t kbps) { canKbps = kbps; } 

    // Ethernet
    uint8_t getEthernetCsPin() const { return ethernetCsPin; }
    uint8_t getEthernetRstPin() const { return ethernetRstPin; }
    uint8_t getEthernetSckPin() const { return ethernetSckPin; }
    uint8_t getEthernetMisoPin() const { return ethernetMisoPin; }
    uint8_t getEthernetMosiPin() const { return ethernetMosiPin; }
    uint32_t getEthernetFrequency() const { return ethernetFrequency; }
    uint8_t getEthernetIrqPin() const { return ethernetIrqPin; }
    const std::array<uint8_t,6>& getEthernetMac() const { return ethernetMac; }

    void setEthernetCsPin(uint8_t pin) { ethernetCsPin = pin; }
    void setEthernetRstPin(uint8_t pin) { ethernetRstPin = pin; }
    void setEthernetSckPin(uint8_t pin) { ethernetSckPin = pin; }
    void setEthernetMisoPin(uint8_t pin) { ethernetMisoPin = pin; }
    void setEthernetMosiPin(uint8_t pin) { ethernetMosiPin = pin; }
    void setEthernetFrequency(uint32_t freq) { ethernetFrequency = freq; }
    void setEthernetIrqPin(uint8_t pin) { ethernetIrqPin = pin; }
    void setEthernetMac(const std::array<uint8_t,6>& mac) { ethernetMac = mac; }

    // SubGHz
    uint8_t getSubGhzSckPin() const { return subGhzSckPin; }
    uint8_t getSubGhzMisoPin() const { return subGhzMisoPin; }
    uint8_t getSubGhzMosiPin() const { return subGhzMosiPin; }
    uint8_t getSubGhzCsPin() const { return subGhzCsPin; }
    uint8_t getSubGhzGdoPin() const { return subGhzGdoPin; }
    float getSubGhzFrequency() const { return subGhzFrequency; }

    void setSubGhzSckPin(uint8_t pin) { subGhzSckPin = pin; }
    void setSubGhzMisoPin(uint8_t pin) { subGhzMisoPin = pin; }
    void setSubGhzMosiPin(uint8_t pin) { subGhzMosiPin = pin; }
    void setSubGhzCsPin(uint8_t pin) { subGhzCsPin = pin; }
    void setSubGhzGdoPin(uint8_t pin) { subGhzGdoPin = pin; }
    void setSubGhzFrequency(float freq) { subGhzFrequency = freq; }

    // NRF24
    uint8_t getRf24CsnPin() const { return rf24CsnPin; }
    uint8_t getRf24CePin() const { return rf24CePin; }
    uint8_t getRf24SckPin() const { return rf24SckPin; }
    uint8_t getRf24MisoPin() const { return rf24MisoPin; }
    uint8_t getRf24MosiPin() const { return rf24MosiPin; }

    void setRf24CsnPin(uint8_t pin) { rf24CsnPin = pin; }
    void setRf24CePin(uint8_t pin) { rf24CePin = pin; }
    void setRf24SckPin(uint8_t pin) { rf24SckPin = pin; }
    void setRf24MisoPin(uint8_t pin) { rf24MisoPin = pin; }
    void setRf24MosiPin(uint8_t pin) { rf24MosiPin = pin; }

    uint8_t getLoRaSckPin() const { return loRaSckPin; }
    uint8_t getLoRaMisoPin() const { return loRaMisoPin; }
    uint8_t getLoRaMosiPin() const { return loRaMosiPin; }
    uint8_t getLoRaCsPin() const { return loRaCsPin; }
    uint8_t getLoRaRstPin() const { return loRaRstPin; }
    uint8_t getLoRaBusyPin() const { return loRaBusyPin; }
    uint8_t getLoRaDio1Pin() const { return loRaDio1Pin; }
    float getLoRaFrequency() const { return loRaFrequency; }
    float getLoRaTcxoVoltage() const { return loRaTcxoVoltage; }
    uint16_t getLoRaBandwidth() const { return loRaBandwidth; }
    uint8_t getLoRaSpreadingFactor() const { return loRaSpreadingFactor; }
    uint8_t getLoRaCodingRate() const { return loRaCodingRate; }
    int8_t getLoRaPower() const { return loRaPower; }
    uint16_t getLoRaPreambleLength() const { return loRaPreambleLength; }
    uint16_t getLoRaSyncWord() const { return loRaSyncWord; }
    bool getLoRaCrc() const { return loRaCrc; }
    bool getLoRaInvertIq() const { return loRaInvertIq; }
    LoRaRadioProfile getLoRaProfile() const {
        return {loRaFrequency, loRaBandwidth, loRaSpreadingFactor,
                loRaCodingRate, loRaPower, loRaTcxoVoltage,
                loRaPreambleLength, loRaSyncWord, loRaCrc, loRaInvertIq};
    }
    void setLoRaSckPin(uint8_t v) { loRaSckPin = v; }
    void setLoRaMisoPin(uint8_t v) { loRaMisoPin = v; }
    void setLoRaMosiPin(uint8_t v) { loRaMosiPin = v; }
    void setLoRaCsPin(uint8_t v) { loRaCsPin = v; }
    void setLoRaRstPin(uint8_t v) { loRaRstPin = v; }
    void setLoRaBusyPin(uint8_t v) { loRaBusyPin = v; }
    void setLoRaDio1Pin(uint8_t v) { loRaDio1Pin = v; }
    void setLoRaFrequency(float v) { loRaFrequency = v; }
    void setLoRaTcxoVoltage(float v) { loRaTcxoVoltage = v; }
    void setLoRaBandwidth(uint16_t v) { loRaBandwidth = v; }
    void setLoRaSpreadingFactor(uint8_t v) { loRaSpreadingFactor = v; }
    void setLoRaCodingRate(uint8_t v) { loRaCodingRate = v; }
    void setLoRaPower(int8_t v) { loRaPower = v; }
    void setLoRaPreambleLength(uint16_t v) { loRaPreambleLength = v; }
    void setLoRaSyncWord(uint16_t v) { loRaSyncWord = v; }
    void setLoRaCrc(bool v) { loRaCrc = v; }
    void setLoRaInvertIq(bool v) { loRaInvertIq = v; }

    // RFID (I2C PN532)
    uint8_t getRfidSdaPin() const { return rfidSdaPin; }
    uint8_t getRfidSclPin() const { return rfidSclPin; }

    void setRfidSdaPin(uint8_t pin) { rfidSdaPin = pin; }
    void setRfidSclPin(uint8_t pin) { rfidSclPin = pin; }

    // SD Card
    uint8_t getSdCardCsPin() const { return sdCardCsPin; }
    uint8_t getSdCardClkPin() const { return sdCardClkPin; }
    uint8_t getSdCardMisoPin() const { return sdCardMisoPin; }
    uint8_t getSdCardMosiPin() const { return sdCardMosiPin; }
    uint32_t getSdCardFrequency() const { return sdCardFrequency; }
    bool getHasInternalSdCard() const { return hasInternalSdCard; }

    void setSdCardCsPin(uint8_t pin) { sdCardCsPin = pin; }
    void setSdCardClkPin(uint8_t pin) { sdCardClkPin = pin; }
    void setSdCardMisoPin(uint8_t pin) { sdCardMisoPin = pin; }
    void setSdCardMosiPin(uint8_t pin) { sdCardMosiPin = pin; }
    void setSdCardFrequency(uint32_t freq) { sdCardFrequency = freq; }

    // USB
    const char* getUSBProductString() const { return usbProductString; }
    const char* getUSBManufacturerString() const { return usbManufacturerString; }
    const char* getUSBSerialString() const { return usbSerialString; }
    uint16_t getUSBVid() const { return usbVid; }
    uint16_t getUSBPid() const { return usbPid; }
    const char* getWebUSBString() const { return webUSBString; }
    
    // SD File Limits
    size_t getFileCountLimit() const { return fileCountLimit; }
    size_t getFileCacheLimit() const { return fileCacheLimit; }

    // NVS
    const char* getNvsPasswordField() const { return nvsPasswordField; }
    const char* getNvsSsidField() const { return nvsSsidField; }

    const char* getNvsHostnameField() const { return nvsHostnameField; }

    // Protected
    const std::vector<uint8_t>& getProtectedPins() const {
        return protectedPins;
    }

    bool isPinProtected(uint8_t pin) const {
        return std::find(protectedPins.begin(), protectedPins.end(), pin) != protectedPins.end();
    }

    bool isPinAnalog(uint8_t pin) const {
        return ((pin > 0) && (pin < 19));
    }

    // Constructor
    GlobalState() {
        #ifdef SPI_CS_PIN
            spiCSPin = SPI_CS_PIN;
        #endif
        #ifdef SPI_CLK_PIN
            spiCLKPin = SPI_CLK_PIN;
        #endif
        #ifdef SPI_MISO_PIN
            spiMISOPin = SPI_MISO_PIN;
        #endif
        #ifdef SPI_MOSI_PIN
            spiMOSIPin = SPI_MOSI_PIN;
        #endif
        #ifdef ONEWIRE_PIN
            oneWirePin = ONEWIRE_PIN;
        #endif
        #ifdef UART_BAUD
            uartBaudRate = UART_BAUD;
        #endif
        #ifdef UART_RX_PIN
            uartRxPin = UART_RX_PIN;
        #endif
        #ifdef UART_TX_PIN
            uartTxPin = UART_TX_PIN;
        #endif
        #ifdef HDUART_BAUD
            hdUartBaudRate = HDUART_BAUD;
        #endif
        #ifdef HDUART_PIN
            hdUartPin = HDUART_PIN;
        #endif
        #ifdef I2C_SCL_PIN
            i2cSclPin = I2C_SCL_PIN;
        #endif
        #ifdef I2C_SDA_PIN
            i2cSdaPin = I2C_SDA_PIN;
        #endif
        #ifdef I2C_FREQ
            i2cFrequency = I2C_FREQ;
        #endif
        #ifdef IR_TX_PIN
            infraredPin = IR_TX_PIN;
        #endif
        #ifdef IR_RX_PIN
            infraredRxPin = IR_RX_PIN;
        #endif
        #ifdef I2S_BCLK_PIN
            i2sBclkPin = I2S_BCLK_PIN;
        #endif
        #ifdef I2S_LRCK_PIN
            i2sLrckPin = I2S_LRCK_PIN;
        #endif
        #ifdef I2S_DATA_PIN
            i2sDataPin = I2S_DATA_PIN;
        #endif
        #ifdef I2S_SAMPLE_RATE
            i2sSampleRate = I2S_SAMPLE_RATE;
        #endif
        #ifdef I2S_BITS
            i2sBitsPerSample = I2S_BITS;
        #endif
        #ifdef TWOWIRE_CLK_PIN
            twoWireClkPin = TWOWIRE_CLK_PIN;
        #endif
        #ifdef TWOWIRE_IO_PIN
            twoWireIoPin = TWOWIRE_IO_PIN;
        #endif
        #ifdef TWOWIRE_RST_PIN
            twoWireRstPin = TWOWIRE_RST_PIN;
        #endif
        #ifdef THREEWIRE_CS_PIN
            threeWireCsPin = THREEWIRE_CS_PIN;
        #endif
        #ifdef THREEWIRE_SK_PIN
            threeWireSkPin = THREEWIRE_SK_PIN;
        #endif
        #ifdef THREEWIRE_DI_PIN 
            threeWireDiPin = THREEWIRE_DI_PIN;
        #endif
        #ifdef THREEWIRE_DO_PIN 
            threeWireDoPin = THREEWIRE_DO_PIN;
        #endif
        #ifdef LED_DATA_PIN
            ledDataPin = LED_DATA_PIN;
        #endif
        #ifdef LED_CLOCK_PIN
            ledClockPin = LED_CLOCK_PIN;
        #endif
        #ifdef CAN_CS_PIN
            canCspin = CAN_CS_PIN;
        #endif
        #ifdef CAN_SCK_PIN
            canSckPin = CAN_SCK_PIN;
        #endif
        #ifdef CAN_SI_PIN
            canSiPin = CAN_SI_PIN;
        #endif
        #ifdef CAN_SO_PIN
            canSoPin = CAN_SO_PIN;
        #endif
        #ifdef CAN_KBPS
            canKbps = CAN_KBPS;
        #endif
        #ifdef ETHERNET_CS_PIN
            ethernetCsPin = ETHERNET_CS_PIN;
        #endif
        #ifdef ETHERNET_CLK_PIN
            ethernetSckPin = ETHERNET_CLK_PIN;
        #endif
        #ifdef ETHERNET_MISO_PIN
            ethernetMisoPin = ETHERNET_MISO_PIN;
        #endif
        #ifdef ETHERNET_MOSI_PIN
            ethernetMosiPin = ETHERNET_MOSI_PIN;
        #endif
        #ifdef ETHERNET_IRQ_PIN
            ethernetIrqPin = ETHERNET_IRQ_PIN;
        #endif
        #ifdef SUBGHZ_SCK_PIN
            subGhzSckPin = SUBGHZ_SCK_PIN;
        #endif
        #ifdef SUBGHZ_SO_PIN
            subGhzMisoPin = SUBGHZ_SO_PIN;
        #endif
        #ifdef SUBGHZ_SI_PIN
            subGhzMosiPin = SUBGHZ_SI_PIN;
        #endif
        #ifdef SUBGHZ_CS_PIN
            subGhzCsPin = SUBGHZ_CS_PIN;
        #endif
        #ifdef SUBGHZ_GDO_PIN
            subGhzGdoPin = SUBGHZ_GDO_PIN;
        #endif
        #ifdef I2C_SCL_PIN
            rfidSclPin = I2C_SCL_PIN;
        #endif
        #ifdef I2C_SDA_PIN
            rfidSdaPin = I2C_SDA_PIN;
        #endif
        #ifdef RF24_CSN_PIN
            rf24CsnPin = RF24_CSN_PIN;
        #endif
        #ifdef RF24_CE_PIN
            rf24CePin = RF24_CE_PIN;
        #endif
        #ifdef RF24_SCK_PIN
            rf24SckPin = RF24_SCK_PIN;
        #endif
        #ifdef RF24_MISO_PIN
            rf24MisoPin = RF24_MISO_PIN;
        #endif
        #ifdef RF24_MOSI_PIN
            rf24MosiPin = RF24_MOSI_PIN;
        #endif
        #ifdef LORA_SCK_PIN
            loRaSckPin = LORA_SCK_PIN; loRaMisoPin = LORA_MISO_PIN; loRaMosiPin = LORA_MOSI_PIN;
            loRaCsPin = LORA_CS_PIN; loRaRstPin = LORA_RST_PIN; loRaBusyPin = LORA_BUSY_PIN; loRaDio1Pin = LORA_DIO1_PIN;
        #endif
        #ifdef SDCARD_CS_PIN
            sdCardCsPin = SDCARD_CS_PIN;
        #else 
            sdCardCsPin = spiCSPin;
        #endif
        #ifdef SDCARD_CLK_PIN
            sdCardClkPin = SDCARD_CLK_PIN;
        #else 
            sdCardClkPin = spiCLKPin;
        #endif
        #ifdef SDCARD_MISO_PIN
            sdCardMisoPin = SDCARD_MISO_PIN;
        #else
            sdCardMisoPin = spiMISOPin;
        #endif
        #ifdef SDCARD_MOSI_PIN
            sdCardMosiPin = SDCARD_MOSI_PIN;
        #else
            sdCardMosiPin = spiMOSIPin;
        #endif

        #ifdef PROTECTED_PINS
        {
            protectedPins.clear();
            std::string pinsStr = PROTECTED_PINS;
            std::stringstream ss(pinsStr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                protectedPins.push_back(std::stoi(item));
            }
        }
        #endif

        #ifdef JTAG_SCAN_PINS
        {
            jtagScanPins.clear();
            std::string pinsStr = JTAG_SCAN_PINS;
            std::stringstream ss(pinsStr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                jtagScanPins.push_back((std::stoi(item)));
            }
        }
        #endif
    }
};
