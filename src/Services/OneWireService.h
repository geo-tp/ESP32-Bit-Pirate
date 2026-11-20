#pragma once

#include <OneWire.h>
#include <vector>
#include "Interfaces/IOneWireService.h"
#include "OneWireNg_CurrentPlatform.h"

#define DS2431_FAMILY 0x2D
#define DS2433_FAMILY 0x23
#define DS28EC20_FAMILY 0x43

class OneWireService : public IOneWireService {
public:
    OneWireService();

    void configure(uint8_t pin) override;
    void close() override;
    void releasePin() override;
    void beginPassiveSniff() override;
    int readPinLevel() const override;
    bool reset() override;
    void write(uint8_t data) override;
    void writeBytes(const uint8_t* data, uint8_t len) override;
    uint8_t read() override;
    void readBytes(uint8_t* buffer, uint8_t length) override;
    void skip() override;
    void select(const uint8_t rom[8]) override;
    uint8_t crc8(const uint8_t* data, uint8_t len) override;
    void resetSearch() override;
    bool search(uint8_t* rom) override;
    std::string executeByteCode(const std::vector<ByteCode>& bytecodes) override;

    // RW1990
    void writeRw1990(uint8_t pin, uint8_t* data, size_t len) override;

    // EEPROM
    void configureEeprom(uint8_t pin) override;
    void closeEeprom() override;
    bool getEepromModelInfos(uint8_t* romId, std::string& model, uint16_t& size, uint8_t& pageSize) override;

    // EEPROM DS24/28
    bool eeprom2431Probe(uint8_t* outId = nullptr) override;
    uint8_t eeprom2431ReadByte(uint16_t address) override;
    std::vector<uint8_t> eeprom2431Dump(uint16_t startAddress, uint16_t length) override;
    bool eeprom2431WriteRow(uint8_t rowAddr, const uint8_t* rowData, bool checkDataIntegrity = true) override;

private:
    OneWire* oneWire = nullptr;
    uint8_t oneWirePin = 0;
    OneWireNg* owEeprom = nullptr;
    OneWireNg::Id eepromId = {};
};
