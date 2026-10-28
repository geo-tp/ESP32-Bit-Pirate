#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Models/ByteCode.h"

class ISpiService {
public:
    virtual ~ISpiService() = default;

    // Optional active-low WP/HOLD lines are held HIGH in master mode; -1 disables them.
    virtual void configure(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t cs, uint32_t frequency = 1000000,
                           int8_t wp = -1, int8_t hold = -1) = 0;
    virtual void end() = 0;
    virtual void beginTransaction() = 0;
    virtual void endTransaction() = 0;
    virtual uint8_t transfer(uint8_t data) = 0;

    virtual std::string readFlashID() = 0;
    virtual void readFlashIdRaw(uint8_t* buffer) = 0;
    virtual void readFlashData(uint32_t address, uint8_t* buffer, size_t length) = 0;
    virtual bool eraseFlashChip(uint32_t freq) = 0;
    // Erases one database-defined block (4/32/64/256 KiB) at an aligned address.
    virtual bool eraseFlashSector(uint32_t address, uint32_t freq) = 0;
    virtual bool writeFlashPage(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq) = 0;
    virtual bool writeFlashPatch(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq) = 0;

    virtual bool initEeprom(uint8_t mosi,
                            uint8_t miso,
                            uint8_t sclk,
                            uint8_t cs,
                            uint16_t pageSize,
                            uint32_t memSize,
                            uint16_t wp = 255,
                            bool small = false) = 0;
    virtual bool probeEeprom() = 0;
    virtual bool writeEeprom(uint32_t address, uint8_t value) = 0;
    virtual uint8_t readEeprom(uint32_t address) = 0;
    virtual bool writeEepromBuffer(uint32_t address, const uint8_t* data, size_t len) = 0;
    virtual bool readEepromBuffer(uint32_t address, uint8_t* buffer, size_t len) = 0;
    virtual bool writeEepromInt(uint32_t address, int32_t value) = 0;
    virtual int32_t readEepromInt(uint32_t address) = 0;
    virtual bool writeEepromFloat(uint32_t address, float value) = 0;
    virtual float readEepromFloat(uint32_t address) = 0;
    virtual bool writeEepromString(uint32_t address, const std::string& str) = 0;
    virtual bool readEepromString(uint32_t address, std::string& str) = 0;
    virtual void eraseEepromChip() = 0;
    virtual void eraseEepromSector(uint32_t address) = 0;
    virtual void eraseEepromPage(uint32_t address) = 0;
    virtual void closeEeprom() = 0;

    virtual void startSlave(int sclk, int miso, int mosi, int cs) = 0;
    virtual void stopSlave(int sclk, int miso, int mosi, int cs) = 0;
    virtual bool isSlave() const = 0;
    virtual std::vector<std::vector<uint8_t>> getSlaveData() = 0;

    virtual std::string executeByteCode(const std::vector<ByteCode>& bytecodes) = 0;
};
