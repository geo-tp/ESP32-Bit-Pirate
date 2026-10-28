#ifndef NVS_SERVICE_H
#define NVS_SERVICE_H

#if __has_include("Vendors/Preferences.h")
#include "Vendors/Preferences.h"
#else
#include <Preferences.h>
#endif
#include <cstdint>
#include <string>
#include "Enums/OneShotBootMode.h"
#include "Interfaces/INvsService.h"

class NvsService : public INvsService {
public:
    ~NvsService() override;

    // Open/Close namespace
    void open() override;
    void close() override;
    bool hasKey(const std::string& key);

    // Read/write string
    void saveString(const std::string& key, const std::string& value) override;
    std::string getString(const std::string& key, const std::string& defaultValue = "") override;

    // Read/write int
    void saveInt(const std::string& key, int value);
    int getInt(const std::string& key, int defaultValue = 0);

    // Utils
    void remove(const std::string& key);
    void clearNamespace();

    // Oneshot boot mode
    void saveOneShotBootMode(OneShotBootMode mode);
    OneShotBootMode getOneShotBootMode();
    void clearOneShotBootMode();
    OneShotBootMode consumeOneShotBootMode();
    void saveOneShotUsbUartBridgeConfig(uint8_t rxPin, uint8_t txPin, bool inverted);
    void getOneShotUsbUartBridgeConfig(uint8_t defaultRxPin, uint8_t defaultTxPin, bool defaultInverted, uint8_t& rxPin, uint8_t& txPin, bool& inverted);
    void clearOneShotUsbUartBridgeConfig();
    void saveOneShotFlashromSerprogConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency, int8_t wpPin = -1, int8_t holdPin = -1);
    void getOneShotFlashromSerprogConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency, int8_t& wpPin, int8_t& holdPin);
    void clearOneShotFlashromSerprogConfig();
    void saveOneShotSumpLogicAnalyzerConfig(const uint8_t* pins, uint8_t channelCount);
    void getOneShotSumpLogicAnalyzerConfig(uint8_t* pins, uint8_t defaultChannelCount, uint8_t& channelCount);
    void clearOneShotSumpLogicAnalyzerConfig();
    void saveOneShotOpenOcdBusPirateConfig(uint8_t tckPin, uint8_t tmsPin, uint8_t tdiPin, uint8_t tdoPin, uint8_t swclkPin, uint8_t swdioPin);
    void getOneShotOpenOcdBusPirateConfig(uint8_t defaultTckPin, uint8_t defaultTmsPin, uint8_t defaultTdiPin, uint8_t defaultTdoPin, uint8_t defaultSwclkPin, uint8_t defaultSwdioPin, uint8_t& tckPin, uint8_t& tmsPin, uint8_t& tdiPin, uint8_t& tdoPin, uint8_t& swclkPin, uint8_t& swdioPin);
    void clearOneShotOpenOcdBusPirateConfig();
    void saveOneShotAvrDudeBusPirateConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency);
    void getOneShotAvrDudeBusPirateConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency);
    void clearOneShotAvrDudeBusPirateConfig();
    void saveOneShotInfraredToyConfig(uint8_t txPin, uint8_t rxPin);
    void getOneShotInfraredToyConfig(uint8_t defaultTxPin, uint8_t defaultRxPin, uint8_t& txPin, uint8_t& rxPin);
    void clearOneShotInfraredToyConfig();
    void saveOneShotSubGhzRawCdcConfig(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin, uint8_t gdo0Pin, float frequencyMhz, int8_t paDbm, uint32_t baudrate);
    void getOneShotSubGhzRawCdcConfig(uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint8_t defaultCsPin, uint8_t defaultGdo0Pin, float defaultFrequencyMhz, int8_t defaultPaDbm, uint32_t defaultBaudrate, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint8_t& csPin, uint8_t& gdo0Pin, float& frequencyMhz, int8_t& paDbm, uint32_t& baudrate);
    void clearOneShotSubGhzRawCdcConfig();
    void saveOneShotBpio2Config(const uint8_t* pins, uint8_t pinCount);
    void getOneShotBpio2Config(const uint8_t* defaultPins, uint8_t* pins, uint8_t pinCount);
    void clearOneShotBpio2Config();

private:
    Preferences preferences;
};

#endif // NVS_SERVICE_H
