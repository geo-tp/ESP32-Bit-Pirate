#include "Services/SpiService.h"
#include "Data/FlashDatabase.h"
#include <ESP32SPISlave.h>
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cstdlib>
#include <memory>

void SpiService::configure(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t cs, uint32_t frequency,
                           int8_t wp, int8_t hold) {
    end();
    csPin = cs;
    spiFrequency = frequency;
    SPI.begin(sclk, miso, mosi, cs);
    // Preload the output latch before enabling outputs (digitalWrite requires
    // pinMode first on Arduino-ESP32 3.x). Keep CS inactive before WP/HOLD.
    gpio_set_level(static_cast<gpio_num_t>(cs), HIGH);
    pinMode(cs, OUTPUT);
    const auto validControlPin = [=](int8_t pin) {
        return pin >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(pin) &&
               pin != mosi && pin != miso && pin != sclk && pin != cs;
    };
    wpPin = validControlPin(wp) ? wp : -1;
    holdPin = validControlPin(hold) && hold != wpPin ? hold : -1;
    for (int8_t pin : {wpPin, holdPin}) {
        if (pin < 0) continue;
        gpio_set_level(static_cast<gpio_num_t>(pin), HIGH);
        pinMode(pin, OUTPUT);
    }
}

void SpiService::end() {
    SPI.end();
    for (int8_t pin : {wpPin, holdPin}) {
        if (pin >= 0) pinMode(pin, INPUT);
    }
    wpPin = holdPin = -1;
}

void SpiService::beginTransaction() {
    SPI.beginTransaction(SPISettings(spiFrequency, MSBFIRST, SPI_MODE0));
    digitalWrite(csPin, LOW);
}

void SpiService::endTransaction() {
    digitalWrite(csPin, HIGH);
    SPI.endTransaction();
}

uint8_t SpiService::transfer(uint8_t data) {
    return SPI.transfer(data);
}

std::string SpiService::readFlashID() {
    uint8_t id[3] = {0};

    beginTransaction();
    SPI.transfer(0x9F);  // JEDEC ID command
    for (uint8_t& byte : id) {
        byte = SPI.transfer(0x00);
    }
    endTransaction();

    char buf[32];
    snprintf(buf, sizeof(buf), "%02X %02X %02X", id[0], id[1], id[2]);
    return std::string(buf);
}

void SpiService::readFlashIdRaw(uint8_t* buffer) {
    beginTransaction();
    SPI.transfer(0x9F);  // JEDEC ID command
    for (int i = 0; i < 3; ++i) {
        buffer[i] = SPI.transfer(0x00);
    }
    endTransaction();
}

void SpiService::readFlashData(uint32_t address, uint8_t* buffer, size_t length) {
    uint8_t id[3];
    readFlashIdRaw(id);
    flashReadAt(findFlashInfo(id[0], id[1], id[2]), spiFrequency, address, buffer, length);
}

bool SpiService::eraseFlashSector(uint32_t address, uint32_t freq) {
    uint8_t id[3];
    readFlashIdRaw(id);
    return flashEraseBlockAt(findFlashInfo(id[0], id[1], id[2]), freq, address);
}

bool SpiService::eraseFlashChip(uint32_t freq) {
    uint8_t id[3];
    readFlashIdRaw(id);
    return flashEraseChipAt(findFlashInfo(id[0], id[1], id[2]), freq);
}

bool SpiService::writeFlashPage(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq) {
    uint8_t id[3];
    readFlashIdRaw(id);
    return flashProgramAt(findFlashInfo(id[0], id[1], id[2]), freq, address, data.data(), data.size());
}

bool SpiService::writeFlashPatch(uint32_t address, const std::vector<uint8_t>& data, uint32_t freq) {
    uint8_t id[3];
    readFlashIdRaw(id);
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const size_t available = flashPatchAvailableMemory(heap_caps_get_free_size(caps),
                                                       heap_caps_get_largest_free_block(caps));
    return flashPatchAt(findFlashInfo(id[0], id[1], id[2]), freq, address, data.data(), data.size(), available);
}

// #### SPI FLASH PROTOCOL ######
// Dedicated transaction at the caller's frequency; independent of the configured bus speed.
void SpiService::flashBeginTransaction(uint32_t frequency) {
    SPI.beginTransaction(SPISettings(frequency, MSBFIRST, SPI_MODE0));
    digitalWrite(csPin, LOW);
}

void SpiService::flashEndTransaction() {
    digitalWrite(csPin, HIGH);
    SPI.endTransaction();
}

void SpiService::flashSendAddress(uint32_t address, bool fourByte) {
    if (fourByte) SPI.transfer((address >> 24) & 0xFF);
    SPI.transfer((address >> 16) & 0xFF);
    SPI.transfer((address >> 8) & 0xFF);
    SPI.transfer(address & 0xFF);
}

// Dedicated 0x13 addresses the entire large chip without changing its address mode.
bool SpiService::flashReadAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t address,
                             uint8_t* buffer, size_t length) {
    if (!buffer || !flashRangeSupported(chip, address, length)) return false;
    const bool fourByte = chip->capacityBytes > flashAddressLimit;
    flashBeginTransaction(frequency);
    SPI.transfer(fourByte ? 0x13 : 0x03);
    flashSendAddress(address, fourByte);
    for (size_t i = 0; i < length; ++i) buffer[i] = SPI.transfer(0x00);
    flashEndTransaction();
    return true;
}

void SpiService::flashCooperate(uint32_t& lastYield) {
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastYield) >= 20) {
        delay(1); // Allow idle/watchdog tasks during long scans and fast writes.
        lastYield = millis();
    }
}

void SpiService::flashCommand(uint32_t frequency, uint8_t opcode) {
    flashBeginTransaction(frequency);
    SPI.transfer(opcode);
    flashEndTransaction();
}

uint8_t SpiService::flashReadStatus(uint32_t frequency) {
    flashBeginTransaction(frequency);
    SPI.transfer(0x05);
    const uint8_t value = SPI.transfer(0);
    flashEndTransaction();
    return value;
}

bool SpiService::flashWaitReady(uint32_t frequency, uint32_t timeout) {
    const uint32_t start = millis();
    while (flashReadStatus(frequency) & 1) {
        if (static_cast<uint32_t>(millis() - start) >= timeout) return false;
        delay(1);
    }
    return true;
}

bool SpiService::flashWriteEnable(uint32_t frequency) {
    if (!flashWaitReady(frequency, flashProgramTimeoutMs)) return false;
    flashCommand(frequency, 0x06);
    return (flashReadStatus(frequency) & 3) == 2; // WEL set, WIP clear.
}

bool SpiService::flashReadBuffer(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr,
                                 uint8_t* buffer, size_t length) {
    uint32_t lastYield = millis();
    for (size_t offset = 0; offset < length;) {
        const size_t count = std::min<size_t>(256, length - offset);
        if (!flashReadAt(chip, frequency, addr + offset, buffer + offset, count)) return false;
        offset += count;
        flashCooperate(lastYield);
    }
    return true;
}

bool SpiService::flashVerify(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr,
                             const uint8_t* expected, size_t length) {
    uint8_t buffer[256];
    size_t offset = 0;
    uint32_t lastYield = millis();
    while (offset < length) {
        const size_t count = std::min<size_t>(sizeof(buffer), length - offset);
        if (!flashReadAt(chip, frequency, addr + offset, buffer, count)) return false;
        for (size_t i = 0; i < count; ++i)
            if (buffer[i] != (expected ? expected[offset + i] : 0xFF)) return false;
        offset += count;
        flashCooperate(lastYield);
    }
    return true;
}

// Program without erase. Preflight the entire request before WREN so an impossible
// 0-to-1 transition never starts a partially programmed request.
bool SpiService::flashProgramAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr,
                                const uint8_t* data, size_t length) {
    if (!data || flashCompatibilityError(chip, FlashOperation::Program) ||
        !flashRangeSupported(chip, addr, length)) return false;
    if (!flashWaitReady(frequency, flashProgramTimeoutMs)) return false;
    uint8_t old[256];
    uint32_t lastYield = millis();
    for (size_t offset = 0; offset < length;) {
        const size_t count = std::min<size_t>(sizeof(old), length - offset);
        if (!flashReadAt(chip, frequency, addr + offset, old, count)) return false;
        for (size_t i = 0; i < count; ++i)
            if ((old[i] & data[offset + i]) != data[offset + i]) return false;
        offset += count;
        flashCooperate(lastYield);
    }
    const bool fourByte = chip->capacityBytes > flashAddressLimit;
    for (size_t offset = 0; offset < length;) {
        const uint32_t current = addr + offset;
        const size_t count = std::min<size_t>(256 - current % 256, length - offset);
        if (!flashWriteEnable(frequency)) return false;
        flashBeginTransaction(frequency);
        SPI.transfer(fourByte ? 0x12 : 0x02);
        flashSendAddress(current, fourByte);
        for (size_t i = 0; i < count; ++i) SPI.transfer(data[offset + i]);
        flashEndTransaction();
        if (!flashWaitReady(frequency, flashProgramTimeoutMs) ||
            !flashVerify(chip, frequency, current, data + offset, count)) return false;
        offset += count;
        flashCooperate(lastYield);
    }
    return true;
}

bool SpiService::flashSetEraseMode(uint32_t frequency, FlashEraseMode mode, bool enter) {
    if (!flashWaitReady(frequency, flashProgramTimeoutMs)) return false;
    if (mode == FlashEraseMode::EnterWriteEnable && !flashWriteEnable(frequency)) return false;
    flashCommand(frequency, enter ? 0xB7 : 0xE9);
    // Some parts retain WEL after a mode command. Leave no armed write latch.
    flashCommand(frequency, 0x04);
    return true;
}

bool SpiService::flashEraseBlockAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr) {
    if (flashCompatibilityError(chip, FlashOperation::BlockErase) ||
        addr % chip->eraseBlockBytes || !flashRangeSupported(chip, addr, chip->eraseBlockBytes)) return false;
    if (!flashWaitReady(frequency, flashBlockEraseTimeoutMs)) return false;
    const bool modeChange = chip->eraseMode != FlashEraseMode::None;
    if (modeChange && !flashSetEraseMode(frequency, chip->eraseMode, true)) return false;
    bool success = flashWriteEnable(frequency);
    if (success) {
        flashBeginTransaction(frequency);
        SPI.transfer(chip->eraseOpcode);
        flashSendAddress(addr, chip->capacityBytes > flashAddressLimit);
        flashEndTransaction();
        success = flashWaitReady(frequency, flashBlockEraseTimeoutMs);
    }
    // Try to leave 4-byte mode even on a write-enable failure or timeout.
    if (modeChange && !flashSetEraseMode(frequency, chip->eraseMode, false)) success = false;
    return success && flashVerify(chip, frequency, addr, nullptr, chip->eraseBlockBytes);
}

bool SpiService::flashEraseChipAt(const FlashChipInfo* chip, uint32_t frequency) {
    if (flashCompatibilityError(chip, FlashOperation::ChipErase)) return false;
    if (!flashWaitReady(frequency, flashChipEraseTimeoutMs) || !flashWriteEnable(frequency)) return false;
    flashCommand(frequency, 0xC7); // No address; independent of 3/4-byte addressing mode.
    return flashWaitReady(frequency, flashChipEraseTimeoutMs) &&
           flashVerify(chip, frequency, 0, nullptr, chip->capacityBytes);
}

// Buffer one verified erase unit (at most 64 KiB), preserving every other byte.
// Validate and allocate before any erase; requests may cross pages, blocks and banks.
bool SpiService::flashPatchAt(const FlashChipInfo* chip, uint32_t frequency, uint32_t addr,
                              const uint8_t* data, size_t length, size_t availableMemory) {
    if (!data || flashCompatibilityError(chip, FlashOperation::Patch) ||
        !flashRangeSupported(chip, addr, length)) return false;
    const uint32_t blockSize = chip->eraseBlockBytes;
    if (blockSize > availableMemory) return false;
    std::unique_ptr<uint8_t[], void (*)(void*)> buffer(static_cast<uint8_t*>(malloc(blockSize)), free);
    if (!buffer || !flashWaitReady(frequency, flashProgramTimeoutMs)) return false;
    size_t offset = 0;
    while (offset < length) {
        const uint32_t current = addr + offset;
        const uint32_t block = current - current % blockSize;
        const size_t within = current - block;
        const size_t count = std::min<size_t>(blockSize - within, length - offset);
        if (!flashReadBuffer(chip, frequency, block, buffer.get(), blockSize)) return false;
        std::copy(data + offset, data + offset + count, buffer.get() + within);
        if (!flashEraseBlockAt(chip, frequency, block) ||
            !flashProgramAt(chip, frequency, block, buffer.get(), blockSize)) return false;
        offset += count;
    }
    return true;
}

std::string SpiService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    bool inTransaction = false;

    for (const auto& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Start:
                if (!inTransaction) {
                    beginTransaction();
                    inTransaction = true;
                }
                break;

            case ByteCodeEnum::Stop:
                if (inTransaction) {
                    endTransaction();
                    inTransaction = false;
                }
                break;

            case ByteCodeEnum::Write:
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    transfer(code.getData());
                }
                break;

            case ByteCodeEnum::Read:
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    uint8_t val = transfer(0x00);  // dummy byte
                    char hex[5];
                    snprintf(hex, sizeof(hex), "%02X ", val);
                    result += hex;
                }
                break;

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

    // Close transaction if left open
    if (inTransaction) {
        endTransaction();
    }

    return result;
}

// #### SPI SLAVE ######

static ESP32SPISlave spiSlave;
static constexpr size_t SLAVE_BUFFER_SIZE = 8;
static std::atomic<bool> slave{false};

static uint8_t slave_tx_buf[SLAVE_BUFFER_SIZE] = {0};
static uint8_t slave_rx_buf[SLAVE_BUFFER_SIZE] = {0};

static std::deque<std::vector<uint8_t>> slaveBuffer;
static std::mutex slaveBufferMutex;

void IRAM_ATTR slaveTransactionCallback(spi_slave_transaction_t* trans, void* arg) {
    if (!slave) return;

    // Copy to vector
    size_t length_bytes = (trans->trans_len + 7) / 8; // trans_len in bits
    std::vector<uint8_t> received(slave_rx_buf, slave_rx_buf + length_bytes);

    {
        // Push thread safe
        std::lock_guard<std::mutex> lock(slaveBufferMutex);
        slaveBuffer.push_back(std::move(received));

        // Limit
        if (slaveBuffer.size() > 100) slaveBuffer.pop_front();
    }
}

void SpiService::startSlave(int sclk, int miso, int mosi, int cs) {
    if (slave) return;
    slave = true;

    spiSlave.setDataMode(SPI_MODE0);
    spiSlave.setQueueSize(1);
    spiSlave.setUserPostTransCbAndArg(slaveTransactionCallback, nullptr);
    spiSlave.begin(FSPI, sclk, miso, mosi, cs);
    
    spiSlave.queue(slave_tx_buf, slave_rx_buf, SLAVE_BUFFER_SIZE);
    spiSlave.trigger();    
}

void SpiService::stopSlave(int sclk, int miso, int mosi, int cs) {
    slave = false;
    
    uint32_t t0 = millis();
    while (!spiSlave.hasTransactionsCompletedAndAllResultsHandled() &&
    (millis() - t0) < 100) {
        delay(1);
    }
    
    delay(100);
    slaveBuffer.clear();
    memset(slave_rx_buf, 0, SLAVE_BUFFER_SIZE);
    memset(slave_tx_buf, 0, SLAVE_BUFFER_SIZE);

    spiSlave.end();
    spi_slave_free(SPI2_HOST);   // FSPI
}

bool SpiService::isSlave() const {
    return slave;
}

std::vector<std::vector<uint8_t>> SpiService::getSlaveData() {
    std::vector<std::vector<uint8_t>> out;

    if (spiSlave.hasTransactionsCompletedAndAllResultsReady(1)) {
        size_t receivedBytes = spiSlave.numBytesReceived();
        if (receivedBytes > SLAVE_BUFFER_SIZE) receivedBytes = SLAVE_BUFFER_SIZE;

        if (receivedBytes > 0) {
            out.emplace_back(slave_rx_buf, slave_rx_buf + receivedBytes);
        }

        // relaunch
        memset(slave_rx_buf, 0, SLAVE_BUFFER_SIZE);
        spiSlave.queue(/*tx*/nullptr, /*rx*/slave_rx_buf, SLAVE_BUFFER_SIZE);
        spiSlave.trigger();
    }

    return out;
}

// #### EEPROM ######

bool SpiService::initEeprom(
    uint8_t mosi, 
    uint8_t miso, uint8_t 
    sclk, uint8_t cs, 
    uint16_t pageSize, 
    uint32_t memSize,  
    uint16_t wp,
    bool small) 
{
    if (eepromInitialized) return true;
    SPI.end();

   // Crée l'objet au moment de l'init (comme tu veux)
    eeprom = new EEPROM_SPI_WE(&SPI, cs, wp, eepromFrequency);

    // Init eeprom
    if (!eeprom->init(sclk, miso, mosi, cs, wp)) return false;

    // Size
    eeprom->setMemorySize((eeprom_size_t)memSize);
    eeprom->setPageSize(
        pageSize == 16  ? EEPROM_PAGE_SIZE_16  :
        pageSize == 32  ? EEPROM_PAGE_SIZE_32  :
        pageSize == 64  ? EEPROM_PAGE_SIZE_64  :
        pageSize == 128 ? EEPROM_PAGE_SIZE_128 :
                          EEPROM_PAGE_SIZE_256
    );

    if (small) {
        eeprom->setSmallEEPROM();
    }

    eepromInitialized = true;
    return true;
}

bool SpiService::probeEeprom() {
    if (!eepromInitialized) return false;
    return eeprom->probe();
}

bool SpiService::writeEeprom(uint32_t address, uint8_t value) {
    if (!eepromInitialized) return false;
    eeprom->write(address, value);
    return true;
}

uint8_t SpiService::readEeprom(uint32_t address) {
    if (!eepromInitialized) return 0xFF;
    return eeprom->read(address);
}

bool SpiService::writeEepromBuffer(uint32_t address, const uint8_t* data, size_t len) {
    if (!eepromInitialized) return false;
    for (size_t i = 0; i < len; ++i) {
        eeprom->write(address + i, data[i]);
    }
    return true;
}

bool SpiService::readEepromBuffer(uint32_t address, uint8_t* buffer, size_t len) {
    if (!eepromInitialized) return false;
    for (size_t i = 0; i < len; ++i) {
        buffer[i] = eeprom->read(address + i);
    }
    return true;
}

bool SpiService::writeEepromInt(uint32_t address, int32_t value) {
    if (!eepromInitialized) return false;
    eeprom->put(address, value);
    return true;
}

int32_t SpiService::readEepromInt(uint32_t address) {
    if (!eepromInitialized) return 0;
    int32_t value = 0;
    eeprom->get(address, value);
    return value;
}

bool SpiService::writeEepromFloat(uint32_t address, float value) {
    if (!eepromInitialized) return false;
    eeprom->put(address, value);
    return true;
}

float SpiService::readEepromFloat(uint32_t address) {
    if (!eepromInitialized) return 0.0f;
    float value = 0.0f;
    eeprom->get(address, value);
    return value;
}

bool SpiService::writeEepromString(uint32_t address, const std::string& str) {
    if (!eepromInitialized) return false;
    String arduinoStr = String(str.c_str()); 
    eeprom->putString(address, arduinoStr);
    return true;
}

bool SpiService::readEepromString(uint32_t address, std::string& str) {
    if (!eepromInitialized) return false;
    String arduinoStr = String(str.c_str());
    eeprom->getString(address, arduinoStr);
    return true;
}

void SpiService::eraseEepromChip() {
    if (eepromInitialized) eeprom->eraseCompleteEEPROM();
}

void SpiService::eraseEepromSector(uint32_t address) {
    if (eepromInitialized) eeprom->eraseSector(address);
}

void SpiService::eraseEepromPage(uint32_t address) {
    if (eepromInitialized) eeprom->erasePage(address);
}

void SpiService::closeEeprom() {
    eepromInitialized = false;

    if (eeprom) {
        delete eeprom;
        eeprom = nullptr;
    }
}
