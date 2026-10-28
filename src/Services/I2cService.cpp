#include "I2cService.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#include "driver/i2c_master.h"
#include "esp32-hal-i2c.h"
#endif

void I2cService::configure(uint8_t sda, uint8_t scl, uint32_t frequency) {
    releaseProbeDevice();
    Wire.end();
    Wire.begin(sda, scl, frequency);
}

void I2cService::beginTransmission(uint8_t address) {
    // Defer Wire.beginTransmission until there is data to send. Otherwise
    // Arduino-ESP32 3.3.x uses i2c_master_probe() for address-only writes,
    // which drives SCL at 100 kHz regardless of Wire.getClock().
    txAddress = address;
    txStarted = false;
}

void I2cService::write(uint8_t data) {
    if (!txStarted) {
        Wire.beginTransmission(txAddress);
        txStarted = true;
    }
    Wire.write(data);
}

bool I2cService::endTransmission(bool sendStop) {
    if (txStarted) {
        txStarted = false;
        return Wire.endTransmission(sendStop) != 0;
    }

    if (!sendStop) {
        // Preserve Arduino's deferred repeated-START transaction semantics.
        // The eventual requestFrom() supplies the actual transfer clock.
        Wire.beginTransmission(txAddress);
        return Wire.endTransmission(false) != 0;
    }

    // II2cService::endTransmission retains Arduino's error convention:
    // false (0) means ACK/success; true means NACK/another error.
    return !probeAddress(txAddress);
}

bool I2cService::probeAddress(uint8_t address) {
    if (address > 0x7F || !i2cIsInit(Wire.getBusNum())) return false;

    const uint32_t speed = Wire.getClock();
    if (!speed) return false;
    if (probeDevice && probeSpeed != speed) releaseProbeDevice();

    if (!probeDevice) {
        auto bus = static_cast<i2c_master_bus_handle_t>(i2cBusHandle(Wire.getBusNum()));
        if (!bus) return false;

        // The address is sent manually in the custom transaction below.
        // A single on-demand handle is reused for all probes (e.g. scan).
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address = I2C_DEVICE_ADDRESS_NOT_USED;
        cfg.scl_speed_hz = speed;
        i2c_master_dev_handle_t device = nullptr;
        if (i2c_master_bus_add_device(bus, &cfg, &device) != ESP_OK) return false;
        probeDevice = device;
        probeSpeed = speed;
    }

    uint8_t addressByte = static_cast<uint8_t>(address << 1); // 7-bit address + WRITE
    i2c_operation_job_t ops[3] = {};
    ops[0].command = I2C_MASTER_CMD_START;
    ops[1].command = I2C_MASTER_CMD_WRITE;
    ops[1].write.data = &addressByte;
    ops[1].write.total_bytes = 1;
    ops[1].write.ack_check = true;
    ops[2].command = I2C_MASTER_CMD_STOP;

    return i2c_master_execute_defined_operations(
        static_cast<i2c_master_dev_handle_t>(probeDevice), ops, 3, Wire.getTimeOut()) == ESP_OK;
}

void I2cService::releaseProbeDevice() const {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    if (probeDevice) {
        i2c_master_bus_rm_device(static_cast<i2c_master_dev_handle_t>(probeDevice));
        probeDevice = nullptr;
        probeSpeed = 0;
    }
#endif
}

uint8_t I2cService::requestFrom(uint8_t address, uint8_t quantity, bool sendStop) {
    return Wire.requestFrom(address, quantity, sendStop);
}

int I2cService::read() {
    return Wire.read();
}

bool I2cService::available() const {
    return Wire.available();
}

bool I2cService::end() const {
    releaseProbeDevice();
    return Wire.end();
}

std::string I2cService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    uint8_t currentAddress = 0;
    bool transmissionStarted = false;
    bool expectAddress = false;

    for (const auto& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Start:
                expectAddress = true;
                break;

            case ByteCodeEnum::Stop:
                if (transmissionStarted) {
                    endTransmission();
                    transmissionStarted = false;
                }
                break;

            case ByteCodeEnum::Write:
                if (expectAddress) {
                    currentAddress = code.getData();
                    beginTransmission(currentAddress);
                    transmissionStarted = true;
                    expectAddress = false;
                } else {
                    if (!transmissionStarted) {
                        beginTransmission(currentAddress);
                        transmissionStarted = true;
                    }
                    for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                        write(code.getData());
                    }
                }
                break;

                case ByteCodeEnum::Read: {
                    if (transmissionStarted) {
                        endTransmission(false);  // release bus
                        transmissionStarted = false;
                    }

                    uint8_t toRead = code.getRepeat();
                    uint8_t readAddr = currentAddress;

                    Wire.requestFrom(readAddr, toRead);
                    
                    for (uint8_t i = 0; i < toRead && Wire.available(); ++i) {
                        uint8_t val = Wire.read();
                        char hex[5];
                        snprintf(hex, sizeof(hex), "%02X ", val);
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

    // If no end stop
    if (transmissionStarted) {
        endTransmission();
    }

    return result;
}

bool I2cService::isReadableDevice(uint8_t addr, uint8_t startReg) {
    beginTransmission(addr);
    write(startReg);
    bool writeOk = (endTransmission(false) == 0);
    if (!writeOk) return false;

    uint8_t received = requestFrom(addr, 1, true);
    if (received != 1 || !available()) return false;

    read();  // read to flush
    return true;
}

/*
Slave
*/

void I2cService::beginSlave(uint8_t address, uint8_t sda, uint8_t scl, uint32_t freq) {
    releaseProbeDevice();
    Wire.end();
    Wire1.end();
    Wire1.begin(address, sda, scl, freq);

    Wire1.onReceive(onSlaveReceive);
    Wire1.onRequest(onSlaveRequest);
}

void I2cService::endSlave() {
    Wire1.end();
}

std::vector<std::string> I2cService::getSlaveLog() {
    portENTER_CRITICAL(&slaveLogMux);
    std::vector<std::string> copy = slaveLog;
    portEXIT_CRITICAL(&slaveLogMux);
    return copy;
}

uint32_t I2cService::getSlaveLogCount() {
    portENTER_CRITICAL(&slaveLogMux);
    uint32_t v = slaveLogTotal;
    portEXIT_CRITICAL(&slaveLogMux);
    return v;
}

void I2cService::clearSlaveLog() {
    portENTER_CRITICAL(&slaveLogMux);
    slaveLog.clear();
    slaveLogWriteIdx = 0;
    portEXIT_CRITICAL(&slaveLogMux);
}

void I2cService::onSlaveReceive(int len) {
    std::string entry = "Master wrote: 0x";
    while (Wire1.available()) {
        uint8_t b = Wire1.read();
        char hex[5];
        snprintf(hex, sizeof(hex), " %02X", b);
        entry += hex;
    }

    portENTER_CRITICAL(&slaveLogMux);

    if (slaveLog.size() < SLAVE_LOG_MAX) {
        slaveLog.push_back(entry);
    } else {
        slaveLog[slaveLogWriteIdx] = entry;
        slaveLogWriteIdx = (slaveLogWriteIdx + 1) % SLAVE_LOG_MAX;
    }
    slaveLogTotal++;

    portEXIT_CRITICAL(&slaveLogMux);
}

void I2cService::onSlaveRequest() {
    portENTER_CRITICAL(&slaveLogMux);

    const std::string entry = "Master requested read";

    if (slaveLog.size() < SLAVE_LOG_MAX) {
        slaveLog.push_back(entry);
    } else {
        slaveLog[slaveLogWriteIdx] = entry;
        slaveLogWriteIdx = (slaveLogWriteIdx + 1) % SLAVE_LOG_MAX;
    }
    slaveLogTotal++;

    portEXIT_CRITICAL(&slaveLogMux);

    // Send counter as data
    uint8_t v = (uint8_t)(slaveLogTotal & 0xFF);
    Wire1.write(&v, 1);
}

/*
Glitch
*/

void I2cService::i2cBitBangDelay(uint32_t delayUs) {
    if (delayUs > 0) esp_rom_delay_us(delayUs);
}

void I2cService::i2cBitBangSetLevel(uint8_t pin, bool level) {
    gpio_set_level((gpio_num_t)pin, level);
}

void I2cService::i2cBitBangSetOutput(uint8_t pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
}

void I2cService::i2cBitBangSetInput(uint8_t pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
}

void I2cService::i2cBitBangStartCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) {
    i2cBitBangSetInput(sda);  // Pull-up
    i2cBitBangSetInput(scl);  // Pull-up
    i2cBitBangDelay(delayUs);

    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, LOW); // SDA low
    i2cBitBangDelay(delayUs);

    i2cBitBangSetOutput(scl);
    i2cBitBangSetLevel(scl, LOW); // SCL low
    i2cBitBangDelay(delayUs);
}

void I2cService::i2cBitBangStopCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) {
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, LOW); // SDA low
    i2cBitBangDelay(delayUs);

    i2cBitBangSetInput(scl);      // SCL high
    i2cBitBangDelay(delayUs);

    i2cBitBangSetInput(sda);      // SDA high
    i2cBitBangDelay(delayUs);
}

void I2cService::i2cBitBangWriteBit(uint8_t scl, uint8_t sda, bool bit, uint32_t d) {
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, bit);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 0);
    i2cBitBangDelay(d);
}

void I2cService::i2cBitBangWriteByte(uint8_t scl, uint8_t sda, uint8_t byte, uint32_t d, bool& ack) {
    for (int i = 7; i >= 0; --i) {
        i2cBitBangWriteBit(scl, sda, (byte >> i) & 0x01, d);
    }

    // ACK/NACK
    i2cBitBangSetInput(sda);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);
    i2cBitBangDelay(d);
    ack = (gpio_get_level((gpio_num_t)sda) == 0); // ACK = SDA low
    i2cBitBangSetLevel(scl, 0);
    i2cBitBangDelay(d);
    i2cBitBangSetOutput(sda);
}

void I2cService::i2cBitBangReadByte(uint8_t scl, uint8_t sda, uint32_t d, bool nackLast) {
    uint8_t data = 0;
    i2cBitBangSetInput(sda);
    for (int i = 7; i >= 0; --i) {
        i2cBitBangSetLevel(scl, 1);
        i2cBitBangDelay(d);
        if (gpio_get_level((gpio_num_t)sda)) {
            data |= (1 << i);
        }
        i2cBitBangSetLevel(scl, 0);
        i2cBitBangDelay(d);
    }

    // Send ACK/NACK
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, nackLast ? 1 : 0); // NACK if last byte
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 0);
    i2cBitBangSetLevel(sda, 1);
}

bool I2cService::i2cBitBangRecoverBus(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    uint32_t delayUs = 500000 / freqHz; // demi periode

    // SCL/SDA input
    i2cBitBangSetInput(scl);
    i2cBitBangSetInput(sda);
    i2cBitBangDelay(delayUs);

    // SDA is low, bus is stuck
    if (gpio_get_level((gpio_num_t)sda) == 0) {
        // 16 pulses on SCL or until SDA is high
        i2cBitBangSetOutput(scl);
        for (int i = 0; i < 16; ++i) {
            i2cBitBangSetLevel(scl, 0);
            i2cBitBangDelay(delayUs);
            i2cBitBangSetLevel(scl, 1);
            i2cBitBangDelay(delayUs);

            if (gpio_get_level((gpio_num_t)sda) == 1) break;
        }
    }

    // STOP condition
    i2cBitBangStopCondition(scl, sda, delayUs);

    delay(20); // wait for bus to stabilize

    return gpio_get_level((gpio_num_t)sda) == 1;
}

void I2cService::rapidStartStop(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    bool ack;
    for (int i = 0; i < 500; ++i) {
        i2cBitBangStartCondition(scl, sda, 0);
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        i2cBitBangStopCondition(scl, sda, 0);
    }
}

void I2cService::floodStart(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    for (int i = 0; i < 1000; ++i) {
        i2cBitBangStartCondition(scl, sda, 0);
        bool ack;
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
    }
}

void I2cService::floodRandom(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    for (int i = 0; i < 100; ++i) {
        // START
        i2cBitBangStartCondition(scl, sda, 0);


        // Send address + data
        bool ack = false;
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        for (int j = 0; j < 20; ++j) {
            i2cBitBangWriteByte(scl, sda, rand() & 0xFF, d, ack);
        }

        // STOP
        i2cBitBangStopCondition(scl, sda, 0);
        delay(5);
    }
}

void I2cService::overReadAttack(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    bool ack;

    // START
    i2cBitBangStartCondition(scl, sda, 0);


    i2cBitBangWriteByte(scl, sda, (address << 1) | 1, d, ack);

    for (int i = 0; i < 1024; ++i) {
        i2cBitBangReadByte(scl, sda, d, false);  // ACK
    }
    i2cBitBangReadByte(scl, sda, d, true); // NACK

    // STOP
    i2cBitBangStopCondition(scl, sda, 0);

}

void I2cService::invalidRegisterRead(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    bool ack;

    for (int i = 0; i < 512; ++i) {
        // START
        i2cBitBangStartCondition(scl, sda, 0);

        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);  // write
        i2cBitBangWriteByte(scl, sda, 0xFF, d, ack);          // invalid register

        // Repeated START
        i2cBitBangSetLevel(sda, 1); i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 0);

        i2cBitBangWriteByte(scl, sda, (address << 1) | 1, d, ack); // read
        i2cBitBangReadByte(scl, sda, d, true); // NACK

        // STOP
        i2cBitBangStopCondition(scl, sda, 0);
        delay(2);
    }
}

void I2cService::simulateClockStretch(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    bool ack;

    for (int i = 0; i < 50; ++i) {
        i2cBitBangSetLevel(sda, 1); i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 0);

        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        i2cBitBangWriteByte(scl, sda, 0xA5, d, ack);

        delay(2); // simulate slave clock stretch confusion

        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 1); i2cBitBangDelay(d);

        delay(2); // simulate slave clock stretch confusion

    }
}

void I2cService::glitchAckInjection(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    uint32_t d = 500000 / freqHz;
    bool ack;

    // START
    i2cBitBangStartCondition(scl, sda, 0);

    i2cBitBangWriteByte(scl, sda, address << 1, d, ack);

    for (int i = 0; i < 10; ++i) {
        for (int b = 7; b >= 0; --b)
            i2cBitBangWriteBit(scl, sda, (0x00 >> b) & 1, d);

        // Simule ACK
        i2cBitBangSetOutput(sda);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(1);
        i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(1);
        i2cBitBangSetLevel(scl, 0);
    }

    // STOP
    i2cBitBangStopCondition(scl, sda, 0);

}

void I2cService::sclSdaGlitch(uint8_t scl, uint8_t sda) {
    for (int i = 0; i < 20; ++i) {
        i2cBitBangSetOutput(scl);
        i2cBitBangSetLevel(scl, 0);
        i2cBitBangSetOutput(sda);
        i2cBitBangSetLevel(sda, 0);
        delayMicroseconds(5 + (esp_random() % 10));  // 5–15 µs

        i2cBitBangSetInput(scl);
        i2cBitBangSetInput(sda);
        delayMicroseconds(5 + (esp_random() % 10));
    }
}

void I2cService::randomClockPulseNoise(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    uint32_t d = 500000 / freqHz;

    i2cBitBangSetOutput(scl);
    i2cBitBangSetOutput(sda);

    for (int i = 0; i < 100; ++i) {
        i2cBitBangSetLevel(scl, random(2));
        i2cBitBangSetLevel(sda, random(2));
        delayMicroseconds(random(d));
    }
}

void I2cService::injectRandomGlitch(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    if (freqHz == 0) freqHz = 100000;

    switch (esp_random() % 3) {

        case 0:
            randomClockPulseNoise(scl, sda, freqHz);
            break;

        case 1:
            sclSdaGlitch(scl, sda);
            break;

        case 2:
            // Reserved address, no real device targeted
            rapidStartStop(0x7F, freqHz, scl, sda);
            break;
    }
}

bool I2cService::initEeprom(uint16_t chipSizeKb, uint8_t addr) {
    eeprom.setMemoryType(chipSizeKb);
    return eeprom.begin(addr);
}

bool I2cService::eepromWriteByte(uint32_t address, uint8_t value) {
    return eeprom.write(address, value);
}

uint8_t I2cService::eepromReadByte(uint32_t address) {
    return eeprom.read(address);
}

bool I2cService::eepromPutString(uint32_t address, const std::string& str) {
    String arduinoStr(str.c_str());
    return eeprom.putString(address, arduinoStr) > 0;
}

bool I2cService::eepromGetString(uint32_t address, std::string& outStr) {
    String arduinoStr;
    eeprom.getString(address, arduinoStr);
    outStr = std::string(arduinoStr.c_str());
    return true;
}

void I2cService::eepromErase(uint8_t fill) {
    eeprom.erase(fill);
}

bool I2cService::eepromDetectMemorySize() {
    uint32_t size = eeprom.detectMemorySizeBytes();
    if (size > 0) {
        eeprom.setMemorySizeBytes(size);
        return true;
    }
    return false;
}

uint8_t I2cService::eepromDetectAddressBytes() {
    uint8_t bytes = eeprom.detectAddressBytes();
    eeprom.setAddressBytes(bytes);
    return bytes;
}

uint16_t I2cService::eepromDetectPageSize() {
    uint16_t size = eeprom.detectPageSizeBytes();
    eeprom.setPageSizeBytes(size);
    return size;
}

uint8_t I2cService::eepromDetectWriteTime(uint8_t testCount) {
    return eeprom.detectWriteTimeMs(testCount);
}


uint32_t I2cService::eepromLength() {
    return eeprom.length();
}

uint32_t I2cService::eepromGetMemorySize()  {
    return eeprom.getMemorySizeBytes();
}

uint16_t I2cService::eepromPageSize()  {
    return eeprom.getPageSizeBytes();
}

uint8_t I2cService::eepromWriteTimeMs() {
    return eeprom.getWriteTimeMs();
}

uint8_t I2cService::eepromAddressBytes() {
    return eeprom.getAddressBytes();
}

bool I2cService::eepromIsConnected() {
    return eeprom.isConnected();
}

bool I2cService::eepromIsBusy() {
    return eeprom.isBusy();
}

bool I2cService::ping(uint8_t addr, bool sendStop, uint32_t* outDtUs) {
    uint32_t t0 = micros();
    beginTransmission(addr);
    uint8_t r = endTransmission(sendStop);
    uint32_t dt = micros() - t0;

    if (outDtUs) *outDtUs = dt;
    return (r == 0);
}

bool I2cService::readReg(uint8_t addr, uint8_t reg, uint8_t* outVal, uint32_t* outDtUs) {
    uint32_t t0 = micros();
    bool ok = false;

    beginTransmission(addr);
    write(reg);

    if (endTransmission(false) == 0) {
        if (requestFrom(addr, (uint8_t)1, true) == 1 && available()) {
            int v = read();
            if (v >= 0) {
                if (outVal) *outVal = (uint8_t)v;
                ok = true;
            }
        }
    }

    // flush
    while (available()) (void)read();

    uint32_t dt = micros() - t0;
    if (outDtUs) *outDtUs = dt;

    return ok;
}

bool I2cService::writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    beginTransmission(addr);
    write(reg);
    write(val);
    return (endTransmission(true) == 0);
}

bool I2cService::probeRegRW(uint8_t addr,
                            uint8_t reg,
                            I2cRegProbeResult& out)
{
    out = {};

    // Read old value
    if (!readReg(addr, reg, &out.oldVal, nullptr)) {
        return false;   // unreadable
    }
    out.readable = true;

    // "Random" test value 
    uint8_t testVal = (uint8_t)(out.oldVal ^ (0xA5u ^ reg));
    if (testVal == out.oldVal) {
        testVal ^= 0xFF; // ensure different value
    }

    // Write test
    if (!writeReg(addr, reg, testVal)) {
        return true;    // readable only
    }
    out.writeOk = true;

    // Read back
    if (!readReg(addr, reg, &out.newVal, nullptr)) {
        (void)writeReg(addr, reg, out.oldVal);
        return true;
    }
    out.readback = true;

    // Restore
    (void)writeReg(addr, reg, out.oldVal);

    // Writable?
    out.rwHit = (out.newVal == testVal);

    return true;
}

bool I2cService::probeReadableReg(uint8_t addr, uint8_t reg) {
    beginTransmission(addr);
    write(reg);
    if (endTransmission(false) != 0) return false;

    uint8_t received = requestFrom(addr, (uint8_t)1, true);
    if (received != 1 || !available()) return false;

    (void)read(); // flush 1
    while (available()) (void)read();
    return true;
}

