#include "NvsService.h"

#include <cstdio>

NvsService::~NvsService() {
    preferences.end(); // Close namespace
}

void NvsService::open() {
    // Open nvs namespace
    preferences.begin(NVS_NAMESPACE, false);
}

void NvsService::close() {
    preferences.end(); // Close namespace
}

bool NvsService::hasKey(const std::string& key) {
    return preferences.isKey(key.c_str());
}

void NvsService::saveString(const std::string& key, const std::string& value) {
    preferences.putString(key.c_str(), value.c_str());
}

std::string NvsService::getString(const std::string& key, const std::string& defaultValue) {
    if (hasKey(key)) {
        return preferences.getString(key.c_str(), defaultValue.c_str()).c_str();
    }
    return defaultValue;
}

void NvsService::saveInt(const std::string& key, int value) {
    preferences.putInt(key.c_str(), value);
}

int NvsService::getInt(const std::string& key, int defaultValue) {
    return preferences.getInt(key.c_str(), defaultValue);
}

void NvsService::remove(const std::string& key) {
    preferences.remove(key.c_str());
}

void NvsService::clearNamespace() {
    preferences.clear();
}

void NvsService::saveOneShotBootMode(OneShotBootMode mode) {
    preferences.putUChar(ONE_SHOT_BOOT_KEY, static_cast<uint8_t>(mode));
}

OneShotBootMode NvsService::getOneShotBootMode() {
    return static_cast<OneShotBootMode>(
        preferences.getUChar(ONE_SHOT_BOOT_KEY, static_cast<uint8_t>(OneShotBootMode::None))
    );
}

void NvsService::clearOneShotBootMode() {
    preferences.remove(ONE_SHOT_BOOT_KEY);
}

OneShotBootMode NvsService::consumeOneShotBootMode() {
    OneShotBootMode mode = getOneShotBootMode();
    if (mode != OneShotBootMode::None) {
        clearOneShotBootMode();
    }
    return mode;
}

void NvsService::saveOneShotUsbUartBridgeConfig(uint8_t rxPin, uint8_t txPin, bool inverted) {
    preferences.putUChar("oneshot_uart_rx", rxPin);
    preferences.putUChar("oneshot_uart_tx", txPin);
    preferences.putBool("uart_inv", inverted);
}

void NvsService::getOneShotUsbUartBridgeConfig(uint8_t defaultRxPin, uint8_t defaultTxPin, bool defaultInverted, uint8_t& rxPin, uint8_t& txPin, bool& inverted) {
    rxPin = preferences.getUChar("oneshot_uart_rx", defaultRxPin);
    txPin = preferences.getUChar("oneshot_uart_tx", defaultTxPin);
    inverted = preferences.getBool("uart_inv", defaultInverted);
}

void NvsService::clearOneShotUsbUartBridgeConfig() {
    preferences.remove("oneshot_uart_rx");
    preferences.remove("oneshot_uart_tx");
    preferences.remove("uart_inv");
}

void NvsService::saveOneShotFlashromSerprogConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency, int8_t wpPin, int8_t holdPin) {
    preferences.putUChar("oneshot_fr_cs", csPin);
    preferences.putUChar("oneshot_fr_sck", sckPin);
    preferences.putUChar("oneshot_fr_miso", misoPin);
    preferences.putUChar("oneshot_fr_mosi", mosiPin);
    preferences.putUInt("oneshot_fr_freq", frequency);
    preferences.putChar("oneshot_fr_wp", wpPin);
    preferences.putChar("oneshot_fr_hold", holdPin);
}

void NvsService::getOneShotFlashromSerprogConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency, int8_t& wpPin, int8_t& holdPin) {
    csPin = preferences.getUChar("oneshot_fr_cs", defaultCsPin);
    sckPin = preferences.getUChar("oneshot_fr_sck", defaultSckPin);
    misoPin = preferences.getUChar("oneshot_fr_miso", defaultMisoPin);
    mosiPin = preferences.getUChar("oneshot_fr_mosi", defaultMosiPin);
    frequency = preferences.getUInt("oneshot_fr_freq", defaultFrequency);
    wpPin = preferences.getChar("oneshot_fr_wp", -1);
    holdPin = preferences.getChar("oneshot_fr_hold", -1);
}

void NvsService::clearOneShotFlashromSerprogConfig() {
    preferences.remove("oneshot_fr_cs");
    preferences.remove("oneshot_fr_sck");
    preferences.remove("oneshot_fr_miso");
    preferences.remove("oneshot_fr_mosi");
    preferences.remove("oneshot_fr_freq");
    preferences.remove("oneshot_fr_wp");
    preferences.remove("oneshot_fr_hold");
}

void NvsService::saveOneShotSumpLogicAnalyzerConfig(const uint8_t* pins, uint8_t channelCount) {
    preferences.putUChar("la_count", channelCount);

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        preferences.putUChar(key.c_str(), pins[i]);
    }
}

void NvsService::getOneShotSumpLogicAnalyzerConfig(uint8_t* pins, uint8_t defaultChannelCount, uint8_t& channelCount) {
    channelCount = preferences.getUChar("la_count", defaultChannelCount);

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        pins[i] = preferences.getUChar(key.c_str(), pins[i]);
    }
}

void NvsService::clearOneShotSumpLogicAnalyzerConfig() {
    preferences.remove("la_count");
    preferences.remove("oneshot_la_rate");
    preferences.remove("la_samples");

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        preferences.remove(key.c_str());
    }
}

void NvsService::saveOneShotOpenOcdBusPirateConfig(uint8_t tckPin, uint8_t tmsPin, uint8_t tdiPin, uint8_t tdoPin, uint8_t swclkPin, uint8_t swdioPin) {
    preferences.putUChar("ocd_tck", tckPin);
    preferences.putUChar("ocd_tms", tmsPin);
    preferences.putUChar("ocd_tdi", tdiPin);
    preferences.putUChar("ocd_tdo", tdoPin);
    size_t wSwclk = preferences.putUChar("ocd_swclk", swclkPin);
    size_t wSwdio = preferences.putUChar("ocd_swdio", swdioPin);
    if (wSwclk == 0 || wSwdio == 0) {
        log_e("NVS write failed: ocd_swclk=%u ocd_swdio=%u", wSwclk, wSwdio);
    }
}

void NvsService::getOneShotOpenOcdBusPirateConfig(uint8_t defaultTckPin, uint8_t defaultTmsPin, uint8_t defaultTdiPin, uint8_t defaultTdoPin, uint8_t defaultSwclkPin, uint8_t defaultSwdioPin, uint8_t& tckPin, uint8_t& tmsPin, uint8_t& tdiPin, uint8_t& tdoPin, uint8_t& swclkPin, uint8_t& swdioPin) {
    tckPin = preferences.getUChar("ocd_tck", defaultTckPin);
    tmsPin = preferences.getUChar("ocd_tms", defaultTmsPin);
    tdiPin = preferences.getUChar("ocd_tdi", defaultTdiPin);
    tdoPin = preferences.getUChar("ocd_tdo", defaultTdoPin);
    swclkPin = preferences.getUChar("ocd_swclk", defaultSwclkPin);
    swdioPin = preferences.getUChar("ocd_swdio", defaultSwdioPin);
}

void NvsService::clearOneShotOpenOcdBusPirateConfig() {
    preferences.remove("ocd_tck");
    preferences.remove("ocd_tms");
    preferences.remove("ocd_tdi");
    preferences.remove("ocd_tdo");
    preferences.remove("ocd_swclk");
    preferences.remove("ocd_swdio");
}

void NvsService::saveOneShotAvrDudeBusPirateConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency) {
    preferences.putUChar("oneshot_avr_cs", csPin);
    preferences.putUChar("oneshot_avr_sck", sckPin);
    preferences.putUChar("avr_miso", misoPin);
    preferences.putUChar("avr_mosi", mosiPin);
    preferences.putUInt("avr_freq", frequency);
}

void NvsService::getOneShotAvrDudeBusPirateConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency) {
    csPin = preferences.getUChar("oneshot_avr_cs", defaultCsPin);
    sckPin = preferences.getUChar("oneshot_avr_sck", defaultSckPin);
    misoPin = preferences.getUChar("avr_miso", defaultMisoPin);
    mosiPin = preferences.getUChar("avr_mosi", defaultMosiPin);
    frequency = preferences.getUInt("avr_freq", defaultFrequency);
}

void NvsService::clearOneShotAvrDudeBusPirateConfig() {
    preferences.remove("oneshot_avr_cs");
    preferences.remove("oneshot_avr_sck");
    preferences.remove("avr_miso");
    preferences.remove("avr_mosi");
    preferences.remove("avr_freq");
}

void NvsService::saveOneShotInfraredToyConfig(uint8_t txPin, uint8_t rxPin) {
    preferences.putUChar("oneshot_irt_tx", txPin);
    preferences.putUChar("oneshot_irt_rx", rxPin);
}

void NvsService::getOneShotInfraredToyConfig(uint8_t defaultTxPin, uint8_t defaultRxPin, uint8_t& txPin, uint8_t& rxPin) {
    txPin = preferences.getUChar("oneshot_irt_tx", defaultTxPin);
    rxPin = preferences.getUChar("oneshot_irt_rx", defaultRxPin);
}

void NvsService::clearOneShotInfraredToyConfig() {
    preferences.remove("oneshot_irt_tx");
    preferences.remove("oneshot_irt_rx");
}

void NvsService::saveOneShotSubGhzRawCdcConfig(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin, uint8_t gdo0Pin, float frequencyMhz, int8_t paDbm, uint32_t baudrate) {
    preferences.putUChar("sgraw_sck", sckPin);
    preferences.putUChar("sgraw_miso", misoPin);
    preferences.putUChar("sgraw_mosi", mosiPin);
    preferences.putUChar("sgraw_cs", csPin);
    preferences.putUChar("sgraw_gdo0", gdo0Pin);
    preferences.putFloat("sgraw_freq", frequencyMhz);
    preferences.putChar("sgraw_pa", paDbm);
    preferences.putUInt("sgraw_baud", baudrate);
}

void NvsService::getOneShotSubGhzRawCdcConfig(uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint8_t defaultCsPin, uint8_t defaultGdo0Pin, float defaultFrequencyMhz, int8_t defaultPaDbm, uint32_t defaultBaudrate, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint8_t& csPin, uint8_t& gdo0Pin, float& frequencyMhz, int8_t& paDbm, uint32_t& baudrate) {
    sckPin = preferences.getUChar("sgraw_sck", defaultSckPin);
    misoPin = preferences.getUChar("sgraw_miso", defaultMisoPin);
    mosiPin = preferences.getUChar("sgraw_mosi", defaultMosiPin);
    csPin = preferences.getUChar("sgraw_cs", defaultCsPin);
    gdo0Pin = preferences.getUChar("sgraw_gdo0", defaultGdo0Pin);
    frequencyMhz = preferences.getFloat("sgraw_freq", defaultFrequencyMhz);
    paDbm = preferences.getChar("sgraw_pa", defaultPaDbm);
    baudrate = preferences.getUInt("sgraw_baud", defaultBaudrate);
}

void NvsService::clearOneShotSubGhzRawCdcConfig() {
    preferences.remove("sgraw_sck");
    preferences.remove("sgraw_miso");
    preferences.remove("sgraw_mosi");
    preferences.remove("sgraw_cs");
    preferences.remove("sgraw_gdo0");
    preferences.remove("sgraw_freq");
    preferences.remove("sgraw_pa");
    preferences.remove("sgraw_baud");
}


void NvsService::saveOneShotBpio2Config(const uint8_t* pins, uint8_t pinCount) {
    if (!pins) return;
    const uint8_t count = pinCount > 8 ? 8 : pinCount;
    char key[16];
    for (uint8_t i = 0; i < count; ++i) {
        std::snprintf(key, sizeof(key), "oneshot_bp2_%u", static_cast<unsigned>(i));
        preferences.putUChar(key, pins[i]);
    }
}

void NvsService::getOneShotBpio2Config(const uint8_t* defaultPins, uint8_t* pins, uint8_t pinCount) {
    if (!defaultPins || !pins) return;
    const uint8_t count = pinCount > 8 ? 8 : pinCount;
    char key[16];
    for (uint8_t i = 0; i < count; ++i) {
        std::snprintf(key, sizeof(key), "oneshot_bp2_%u", static_cast<unsigned>(i));
        pins[i] = preferences.getUChar(key, defaultPins[i]);
    }
}

void NvsService::clearOneShotBpio2Config() {
    char key[16];
    for (uint8_t i = 0; i < 8; ++i) {
        std::snprintf(key, sizeof(key), "oneshot_bp2_%u", static_cast<unsigned>(i));
        preferences.remove(key);
    }
}
