#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Models/ByteCode.h"

class IOneWireService {
public:
    virtual ~IOneWireService() = default;

    virtual void configure(uint8_t pin) = 0;
    virtual void close() = 0;
    virtual void releasePin() = 0;
    virtual void beginPassiveSniff() = 0;
    virtual int readPinLevel() const = 0;
    virtual bool reset() = 0;
    virtual void write(uint8_t data) = 0;
    virtual void writeBytes(const uint8_t* data, uint8_t len) = 0;
    virtual uint8_t read() = 0;
    virtual void readBytes(uint8_t* buffer, uint8_t length) = 0;
    virtual void skip() = 0;
    virtual void select(const uint8_t rom[8]) = 0;
    virtual uint8_t crc8(const uint8_t* data, uint8_t len) = 0;
    virtual void resetSearch() = 0;
    virtual bool search(uint8_t* rom) = 0;
    virtual std::string executeByteCode(const std::vector<ByteCode>& bytecodes) = 0;

    virtual void writeRw1990(uint8_t pin, uint8_t* data, size_t len) = 0;

    virtual void configureEeprom(uint8_t pin) = 0;
    virtual void closeEeprom() = 0;
    virtual bool getEepromModelInfos(uint8_t* romId, std::string& model, uint16_t& size, uint8_t& pageSize) = 0;

    virtual bool eeprom2431Probe(uint8_t* outId = nullptr) = 0;
    virtual uint8_t eeprom2431ReadByte(uint16_t address) = 0;
    virtual std::vector<uint8_t> eeprom2431Dump(uint16_t startAddress, uint16_t length) = 0;
    virtual bool eeprom2431WriteRow(uint8_t rowAddr, const uint8_t* rowData, bool checkDataIntegrity = true) = 0;
};
