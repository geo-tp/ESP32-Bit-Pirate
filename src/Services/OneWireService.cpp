#include "OneWireService.h"

OneWireService::OneWireService() {}

void OneWireService::configure(uint8_t pin) {
    if (oneWire != nullptr) {
        delete oneWire;
        oneWire = nullptr;
    }
    oneWirePin = pin;
    oneWire = new OneWire(pin);
}

void OneWireService::close() {
    if (oneWire) {
        delete oneWire;
        oneWire = nullptr;
    }
}

void OneWireService::releasePin() {
    // Release the pin drive left by the OneWire library's bit-banging
    // (raw GPIO register ops, no pinMode) without destroying the library
    // object: session state must survive mode switches.
    pinMode(oneWirePin, INPUT);
}

void OneWireService::beginPassiveSniff() {
    pinMode(oneWirePin, INPUT);
}

int OneWireService::readPinLevel() const {
    return digitalRead(oneWirePin);
}

bool OneWireService::reset() {
    if (oneWire) return oneWire->reset();
    return false;
}

void OneWireService::write(uint8_t data) {
    if (oneWire) oneWire->write(data);
}

void OneWireService::writeBytes(const uint8_t* data, uint8_t len) {
    if (oneWire) oneWire->write_bytes(data, len);
}

void OneWireService::writeRw1990(uint8_t pin, uint8_t* data, size_t len) {
    // Based on: https://github.com/ArminJo/iButtonProgrammer/blob/master/iButtonProgrammer.ino
    // @ArminJo

    // Configure for bit bang
    pinMode(pin, INPUT_PULLUP);
    delay(10);

    oneWire->reset();
    delay(1);

    // Read is necessary before write
    oneWire->write(0x33);
    for (uint8_t i = 0; i < 8; i++) {
        oneWire->read();
    }
    delay(5);

    // Reset for write
    oneWire->reset();
    delay(1);

    // Write mode
    oneWire->write(0xD5);

    // Bit-bang each byte
    for (size_t byteIndex = 0; byteIndex < len; byteIndex++) {
        uint8_t byte = data[byteIndex];
        for (uint8_t bit = 0; bit < 8; bit++) {
            bool bitValue = byte & 0x01;

            if (bitValue) {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
                delayMicroseconds(60);
                pinMode(pin, INPUT);
                delay(10);
            } else {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
                pinMode(pin, INPUT);
                delay(10);
            }

            byte >>= 1;
        }
    }

    delay(5);
    oneWire->reset();
}

uint8_t OneWireService::read() {
    return oneWire ? oneWire->read() : 0;
}

void OneWireService::readBytes(uint8_t* buffer, uint8_t length) {
    oneWire->read_bytes(buffer, length);
}

void OneWireService::skip() {
    if (oneWire) oneWire->skip();
}

void OneWireService::select(const uint8_t rom[8]) {
    if (oneWire) oneWire->select(rom);
}

uint8_t OneWireService::crc8(const uint8_t* data, uint8_t len) {
    if (!oneWire) return 0;
    return OneWire::crc8(data, len);
}

std::string OneWireService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;

    for (const auto& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Start:
            case ByteCodeEnum::Stop:
                reset();
                break;

            case ByteCodeEnum::Write: {
                uint8_t byte = static_cast<uint8_t>(code.getData());
                oneWire->write(byte);
                break;
            }

            case ByteCodeEnum::Read: {
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    uint8_t value = read();
                    char hex[4];
                    snprintf(hex, sizeof(hex), "%02X ", value);
                    result += hex;
                }
                break;
            }

            case ByteCodeEnum::DelayMs:
                delay(code.getRepeat());
                break;

            case ByteCodeEnum::DelayUs:
                delayMicroseconds(code.getRepeat());
                break;

            default:
                break;
        }
    }
    return result;
}

void OneWireService::resetSearch() {
    if (oneWire) oneWire->reset_search();
}

bool OneWireService::search(uint8_t* rom) {
    if (!oneWire) return false;
    return oneWire->search(rom);
}

void OneWireService::configureEeprom(uint8_t pin) {
    if (owEeprom) {
        delete owEeprom;
        owEeprom = nullptr;
    }

    owEeprom = new OneWireNg_CurrentPlatform(pin, false);  // parasitic=false
}

void OneWireService::closeEeprom() {
    if (owEeprom) {
        delete owEeprom;
        owEeprom = nullptr;
    }
}

bool OneWireService::eeprom2431Probe(uint8_t* outId) {
    if (!owEeprom) return false;

    const uint32_t timeoutMs = 500;
    uint32_t start = millis();

    owEeprom->searchReset();

    OneWireNg::Id id;
    while (millis() - start < timeoutMs) {
        if (owEeprom->search(id) < OneWireNg::EC_SUCCESS) break;

        uint8_t family = id[0];
        if (family == DS2431_FAMILY || family == DS2433_FAMILY || family == DS28EC20_FAMILY) {
            memcpy(eepromId, id, sizeof(OneWireNg::Id));
            if (outId) memcpy(outId, id, sizeof(OneWireNg::Id));
            return true;
        }
    }

    memset(eepromId, 0, sizeof(OneWireNg::Id));
    return false;
}

uint8_t OneWireService::eeprom2431ReadByte(uint16_t address) {
    if (!owEeprom) return 0xFF;

    uint8_t cmd[3] = {
        0xF0,
        static_cast<uint8_t>(address & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF)
    };
    uint8_t value = 0xFF;

    owEeprom->addressSingle(eepromId);
    owEeprom->touchBytes(cmd, 3);
    owEeprom->touchBytes(&value, 1);

    return value;
}

bool OneWireService::eeprom2431WriteRow(uint8_t rowAddr, const uint8_t* rowData, bool checkDataIntegrity) {
    if (!owEeprom || rowAddr > 15) return false;

    constexpr uint8_t ROW_SIZE = 8;
    constexpr uint8_t CMD_WRITE_SCRATCHPAD = 0x0F;
    constexpr uint8_t CMD_READ_SCRATCHPAD  = 0xAA;
    constexpr uint8_t CMD_COPY_SCRATCHPAD  = 0x55;

    uint8_t ta1 = rowAddr * ROW_SIZE;
    uint8_t ta2 = 0x00;

    // WRITE SCRATCHPAD
    uint8_t cmd1[3 + ROW_SIZE] = { CMD_WRITE_SCRATCHPAD, ta1, ta2 };
    memcpy(cmd1 + 3, rowData, ROW_SIZE);

    owEeprom->addressSingle(eepromId);
    owEeprom->touchBytes(cmd1, sizeof(cmd1));

    uint8_t crc1[2];
    owEeprom->readBytes(crc1, 2);
    if (owEeprom->checkInvCrc16(cmd1, sizeof(cmd1), owEeprom->getLSB_u16(crc1)) != OneWireNg::EC_SUCCESS) {
        return false;
    }

    // READ SCRATCHPAD
    uint8_t rsp[1 + 2 + 1 + ROW_SIZE + 2]; // cmd + TA1/TA2 + ES + data + CRC
    rsp[0] = CMD_READ_SCRATCHPAD;
    memset(rsp + 1, 0xFF, sizeof(rsp) - 1);

    owEeprom->resume();
    owEeprom->touchBytes(rsp, sizeof(rsp));

    uint8_t es = rsp[3];
    uint8_t* crc2 = rsp + sizeof(rsp) - 2;

    if (owEeprom->checkInvCrc16(rsp, sizeof(rsp) - 2, owEeprom->getLSB_u16(crc2)) != OneWireNg::EC_SUCCESS) {
        return false;
    }

    if (rsp[1] != ta1 || rsp[2] != ta2 || (es & 0x20) || (es & 0x80)) {
        return false;
    }

    if (checkDataIntegrity && memcmp(rsp + 4, rowData, ROW_SIZE) != 0) {
        return false;
    }

    // COPY SCRATCHPAD
    uint8_t cmd3[4] = { CMD_COPY_SCRATCHPAD, ta1, ta2, es };
    owEeprom->resume();
    owEeprom->touchBytes(cmd3, sizeof(cmd3));

    delay(15); // wait for EEPROM commit

    return true;
}

std::vector<uint8_t> OneWireService::eeprom2431Dump(uint16_t startAddress, uint16_t length) {
    std::vector<uint8_t> data(length, 0xFF);
    if (!owEeprom) return data;

    uint8_t cmd[3] = {
        0xF0,
        static_cast<uint8_t>(startAddress & 0xFF),
        static_cast<uint8_t>((startAddress >> 8) & 0xFF)
    };

    owEeprom->addressSingle(eepromId);
    owEeprom->touchBytes(cmd, 3);
    owEeprom->touchBytes(data.data(), length);

    return data;
}

bool OneWireService::getEepromModelInfos(uint8_t* romId, std::string& model, uint16_t& size, uint8_t& pageSize) {
    if (!romId) return false;

    uint8_t familyCode = romId[0];
    model = "Unknown";
    size = 128;    // fallback default
    pageSize = 8;  // fallback default

    switch (familyCode) {
        case DS2431_FAMILY:
            model = "DS2431 (1 Kbit)";
            size = 128;
            pageSize = 8;
            return true;
        case DS2433_FAMILY:
            model = "DS2433 (4 Kbit)";
            size = 512;
            pageSize = 32;
            return true;
        case DS28EC20_FAMILY:
            model = "DS28EC20 (20 Kbit)";
            size = 2560;
            pageSize = 32;
            return true;
        default:
            return false;
    }
}
