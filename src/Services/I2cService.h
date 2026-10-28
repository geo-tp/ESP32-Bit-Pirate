#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include "Interfaces/II2cService.h"
#include <SparkFun_External_EEPROM.h>

class I2cService : public II2cService {
public:
    // Base
    void configure(uint8_t sda, uint8_t scl, uint32_t frequency = 100000) override;
    void beginTransmission(uint8_t address) override;
    void write(uint8_t data) override;
    bool endTransmission(bool sendStop = true) override;
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool sendStop = true) override;
    int read() override;
    bool available() const override;
    bool end() const override;
    bool isReadableDevice(uint8_t addr, uint8_t startReg) override;
    bool ping(uint8_t addr, bool sendStop = true, uint32_t* outDtUs = nullptr) override;
    bool readReg(uint8_t addr, uint8_t reg, uint8_t* outVal, uint32_t* outDtUs = nullptr) override;
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) override;
    bool probeRegRW(uint8_t addr, uint8_t reg, I2cRegProbeResult& out) override;

    // I2C Bit bang
    void i2cBitBangDelay(uint32_t delayUs) override;
    void i2cBitBangSetLevel(uint8_t pin, bool level) override;
    void i2cBitBangSetOutput(uint8_t pin) override;
    void i2cBitBangSetInput(uint8_t pin) override;
    void i2cBitBangWriteBit(uint8_t scl, uint8_t sda, bool bit, uint32_t delayUs) override;
    void i2cBitBangWriteByte(uint8_t scl, uint8_t sda, uint8_t data, uint32_t delayUs, bool& ack) override;
    void i2cBitBangReadByte(uint8_t scl, uint8_t sda, uint32_t delayUs, bool nackLast) override;
    void i2cBitBangStartCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) override;
    void i2cBitBangStopCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) override;
    bool i2cBitBangRecoverBus(uint8_t scl, uint8_t sda, uint32_t freqHz) override;

    // Slave
    void beginSlave(uint8_t address, uint8_t sda, uint8_t scl, uint32_t freq = 100000) override;
    void endSlave() override;
    std::vector<std::string> getSlaveLog() override;
    uint32_t getSlaveLogCount() override;
    void clearSlaveLog() override;

    // Glitch
    void rapidStartStop(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void floodRandom(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void floodStart(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) override;
    void overReadAttack(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void invalidRegisterRead(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void simulateClockStretch(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void sclSdaGlitch(uint8_t sclPin, uint8_t sdaPin) override;
    void randomClockPulseNoise(uint8_t sclPin, uint8_t sdaPin, uint32_t freqHz) override;
    void glitchAckInjection(uint8_t address, uint32_t freqHz, uint8_t sclPin, uint8_t sdaPin) override;
    void injectRandomGlitch(uint8_t sclPin, uint8_t sdaPin, uint32_t freqHz) override;

    // Instructions
    std::string executeByteCode(const std::vector<ByteCode>& bytecodes) override;

    // EEPROM
    bool initEeprom(uint16_t chipSizeKb = 512, uint8_t addr = 0x50) override;
    bool eepromWriteByte(uint32_t address, uint8_t value) override;
    uint8_t eepromReadByte(uint32_t address) override;
    bool eepromPutString(uint32_t address, const std::string& str) override;
    bool eepromGetString(uint32_t address, std::string& outStr) override;
    uint32_t eepromLength() override;
    uint32_t eepromGetMemorySize() override;
    uint16_t eepromPageSize() override;
    uint8_t eepromWriteTimeMs() override;
    uint8_t eepromAddressBytes() override;
    bool eepromIsConnected() override;
    bool eepromIsBusy() override;
    void eepromErase(uint8_t fill = 0xFF) override;
    bool eepromDetectMemorySize() override;
    uint8_t eepromDetectAddressBytes() override;
    uint16_t eepromDetectPageSize() override;
    uint8_t eepromDetectWriteTime(uint8_t testCount = 8) override;

private:
    mutable void* probeDevice = nullptr;
    mutable uint32_t probeSpeed = 0;
    uint8_t txAddress = 0;
    bool txStarted = false;
    bool probeAddress(uint8_t address);
    void releaseProbeDevice() const;

    ExternalEEPROM eeprom;
    bool probeReadableReg(uint8_t addr, uint8_t reg);

    static void onSlaveReceive(int len);
    static void onSlaveRequest();
    inline static std::vector<std::string> slaveLog = {};
    inline static size_t slaveLogWriteIdx = 0;
    inline static uint32_t slaveLogTotal = 0;
    inline static portMUX_TYPE slaveLogMux = portMUX_INITIALIZER_UNLOCKED;
};
