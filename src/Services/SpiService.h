#pragma once

#include <vector>
#include <atomic>
#include <deque>
#include <mutex>
#include <Arduino.h>
#include <EEPROM_SPI_WE.h>
#include <SPI.h>
#include <Data/FlashDatabase.h>
#include <Interfaces/ISpiService.h>
#include <Models/ByteCode.h>

class SpiService : public ISpiService {
public:
    // Base
    void configure(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t cs, uint32_t frequency = 1000000,
                   int8_t wp = -1, int8_t hold = -1);
    void end();
    void beginTransaction();
    void endTransaction();
    uint8_t transfer(uint8_t data);

    // Flash
    std::string readFlashID();
    void readFlashIdRaw(uint8_t* buffer);
    void readFlashData(uint32_t address, uint8_t* buffer, size_t length);
    bool eraseFlashChip(uint32_t freq);
    bool eraseFlashSector(uint32_t address, uint32_t freq);
    bool writeFlashPage(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq);
    bool writeFlashPatch(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq);

    // EEPROM
    bool initEeprom(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t cs, uint16_t pageSize, uint32_t memSize, uint16_t wp=255, bool small=false);
    bool probeEeprom();
    bool writeEeprom(uint32_t address, uint8_t value);
    uint8_t readEeprom(uint32_t address);
    bool writeEepromBuffer(uint32_t address, const uint8_t* data, size_t len);
    bool readEepromBuffer(uint32_t address, uint8_t* buffer, size_t len);
    bool writeEepromInt(uint32_t address, int32_t value);
    int32_t readEepromInt(uint32_t address);
    bool writeEepromFloat(uint32_t address, float value);
    float readEepromFloat(uint32_t address);
    bool writeEepromString(uint32_t address, const std::string& str);
    bool readEepromString(uint32_t address, std::string& str);
    void eraseEepromChip();
    void eraseEepromSector(uint32_t address);
    void eraseEepromPage(uint32_t address);
    void closeEeprom();

    // Slave
    void startSlave(int sclk, int miso, int mosi, int cs);
    void stopSlave(int sclk, int miso, int mosi, int cs);
    bool isSlave() const;
    std::vector<std::vector<uint8_t>> getSlaveData();

    // Instructions
    std::string executeByteCode(const std::vector<ByteCode>& bytecodes);
private:
    uint8_t csPin;
    int8_t wpPin = -1;
    int8_t holdPin = -1;
    uint32_t spiFrequency = 1000000;
    EEPROM_SPI_WE* eeprom = nullptr;
    bool eepromInitialized = false;
    uint32_t eepromFrequency = 8000000;

    // JEDEC SPI NOR flash protocol (commands, program/erase/verify with WEL/WIP handling).
    static constexpr uint32_t flashProgramTimeoutMs = 3000;
    static constexpr uint32_t flashBlockEraseTimeoutMs = 120000;
    static constexpr uint32_t flashChipEraseTimeoutMs = 600000;

    void flashBeginTransaction(uint32_t frequency);
    void flashEndTransaction();
    void flashSendAddress(uint32_t address, bool fourByte);
    bool flashReadAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t address, uint8_t* buffer, size_t length);
    void flashCommand(uint32_t frequency, uint8_t opcode);
    uint8_t flashReadStatus(uint32_t frequency);
    bool flashWaitReady(uint32_t frequency, uint32_t timeoutMs);
    bool flashWriteEnable(uint32_t frequency);
    bool flashReadBuffer(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr, uint8_t* buffer, size_t length);
    bool flashVerify(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr, const uint8_t* expected, size_t length);
    bool flashProgramAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr, const uint8_t* data, size_t length);
    bool flashSetEraseMode(uint32_t frequency, FlashEraseMode mode, bool enter);
    bool flashEraseBlockAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr);
    bool flashEraseChipAt(const FlashChipInfo* chip, uint32_t frequency);
    bool flashPatchAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr,
                       const uint8_t* data, size_t length, size_t availableMemory);
    static void flashCooperate(uint32_t& lastYield);
};
