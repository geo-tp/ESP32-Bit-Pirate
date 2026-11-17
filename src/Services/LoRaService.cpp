#include "Services/LoRaService.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#ifdef DEVICE_CARDPUTERADV
#include <Wire.h>
#endif

LoRaService* LoRaService::instance_ = nullptr;

uint32_t LoRaService::bandwidthIndex(float bandwidth) {
    if (bandwidth >= 375.0f) return 2;
    if (bandwidth >= 187.5f) return 1;
    return 0;
}

RadioTcxoCtrlVoltage_t LoRaService::tcxoControlVoltage(float voltage) {
    if (voltage < 1.65f) return TCXO_CTRL_1_6V;
    if (voltage < 1.75f) return TCXO_CTRL_1_7V;
    if (voltage < 2.00f) return TCXO_CTRL_1_8V;
    if (voltage < 2.30f) return TCXO_CTRL_2_2V;
    if (voltage < 2.55f) return TCXO_CTRL_2_4V;
    if (voltage < 2.85f) return TCXO_CTRL_2_7V;
    if (voltage < 3.15f) return TCXO_CTRL_3_0V;
    return TCXO_CTRL_3_3V;
}

uint8_t LoRaService::cadDetectionPeak(uint8_t spreadingFactor) {
    switch (spreadingFactor) {
        case 6:
        case 7:
        case 8:  return 22;
        case 9:  return 23;
        case 10: return 24;
        case 11: return 25;
        case 12: return 28;
        default: return 23;
    }
}

bool LoRaService::waitBusyLow(uint8_t busy, uint32_t timeoutMs) {
    const uint32_t started = millis();
    while (digitalRead(busy) == HIGH) {
        if (millis() - started >= timeoutMs) return false;
        delay(1);
    }
    return true;
}

uint8_t LoRaService::probeTransfer(uint8_t value, uint8_t sck,
                                   uint8_t miso, uint8_t mosi) {
    uint8_t received = 0;
    for (int bit = 7; bit >= 0; --bit) {
        digitalWrite(mosi, (value >> bit) & 1u);
        delayMicroseconds(1);
        digitalWrite(sck, HIGH);
        received = static_cast<uint8_t>(
            (received << 1) | (digitalRead(miso) == HIGH ? 1u : 0u));
        delayMicroseconds(1);
        digitalWrite(sck, LOW);
    }
    return received;
}

bool LoRaService::probeRadio(uint8_t sck, uint8_t miso, uint8_t mosi,
                             uint8_t cs, uint8_t rst, uint8_t busy,
                             uint8_t dio1) {
    // Probe without touching the shared SPIClass. When no radio is attached
    pinMode(sck, OUTPUT);
    digitalWrite(sck, LOW);
    pinMode(mosi, OUTPUT);
    digitalWrite(mosi, LOW);
    pinMode(miso, INPUT);

    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH);
    pinMode(busy, INPUT);
    pinMode(dio1, INPUT);

    // Hardware reset
    digitalWrite(rst, LOW);
    delay(10);
    digitalWrite(rst, HIGH);
    delay(20);

    if (!waitBusyLow(busy, 150)) {
        lastError_ = -90;
        return false;
    }

    // Read REG_LR_SYNCWORD (0x0740) directly. After reset an SX1262 should
    // expose either the private LoRa value 0x1424 or public value 0x3444
    uint8_t syncMsb = 0;
    uint8_t syncLsb = 0;
    digitalWrite(cs, LOW);
    probeTransfer(0x1D, sck, miso, mosi); // RADIO_READ_REGISTER
    probeTransfer(0x07, sck, miso, mosi);
    probeTransfer(0x40, sck, miso, mosi);
    probeTransfer(0x00, sck, miso, mosi);
    syncMsb = probeTransfer(0x00, sck, miso, mosi);
    syncLsb = probeTransfer(0x00, sck, miso, mosi);
    digitalWrite(cs, HIGH);

    const bool validSync =
        (syncMsb == 0x14 && syncLsb == 0x24) ||
        (syncMsb == 0x34 && syncLsb == 0x44);

    if (!validSync) {
        lastError_ = -91;
        return false;
    }

    // The caller now knows a real SX1262 is present
    return true;
}

void LoRaService::releaseRadioPins() {
    if (!pinsAssigned_) return;

    // Return every external-radio pin to a passive state
    pinMode(sckPin_, INPUT);
    pinMode(misoPin_, INPUT);
    pinMode(mosiPin_, INPUT);
    pinMode(csPin_, INPUT);
    pinMode(rstPin_, INPUT);
    pinMode(busyPin_, INPUT);
    pinMode(dio1Pin_, INPUT);
    pinsAssigned_ = false;
}

void LoRaService::releaseSpiBus() {
    if (spi_) spi_->end();
    set_lora_spi_instance(nullptr);
    releaseRadioPins();
    spi_ = nullptr;
}

#ifdef DEVICE_CARDPUTERADV

#define PI4IO_ADDR 0x43
#define PI4IO_REG_IO_DIR 0x03
#define PI4IO_REG_OUT_SET 0x05
#define PI4IO_REG_OUT_H_IM 0x07

TwoWire *LoRaService::findLoraCapBus()
{
    TwoWire *candidates[] = {&Wire1, &Wire};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        candidates[i]->beginTransmission(PI4IO_ADDR);
        if (candidates[i]->endTransmission() == 0) {
            return candidates[i];
        }
    }
    return nullptr;
}

bool LoRaService::pi4ioWrite(TwoWire *bus, uint8_t reg, uint8_t val)
{
    bus->beginTransmission(PI4IO_ADDR);
    bus->write(reg);
    bus->write(val);
    uint8_t status = bus->endTransmission();
    if (status != 0) {
        Serial.println("!!! PI4IO write error !!!");
        return false;
    }
    return true;
}

void LoRaService::initLoraCap()
{
    Serial.println("Init LoraCap");
    TwoWire *bus = findLoraCapBus();
    if (!bus) {
        return;
    }
    pi4ioWrite(bus, PI4IO_REG_IO_DIR, 0b00000001);
    pi4ioWrite(bus, PI4IO_REG_OUT_H_IM, 0b00000000);
    pi4ioWrite(bus, PI4IO_REG_OUT_SET, 0b00000001);
}

void LoRaService::deInitLoraCap()
{
    TwoWire *bus = findLoraCapBus();
    if (!bus) {
        return;
    }
    pi4ioWrite(bus, PI4IO_REG_IO_DIR, 0b00000001);
    pi4ioWrite(bus, PI4IO_REG_OUT_H_IM, 0b00000000);
    pi4ioWrite(bus, PI4IO_REG_OUT_SET, 0b00000000);
}
#endif

bool LoRaService::configure(SPIClass& spi, uint8_t sck, uint8_t miso, uint8_t mosi,
                            uint8_t cs, uint8_t rst, uint8_t busy, uint8_t dio1,
                            const LoRaRadioProfile& profile) {
    deinitRfModule();

    spi_ = &spi;
    sckPin_ = sck;
    misoPin_ = miso;
    mosiPin_ = mosi;
    csPin_ = cs;
    rstPin_ = rst;
    busyPin_ = busy;
    dio1Pin_ = dio1;
    pinsAssigned_ = true;
    hardwareInitialized_ = false;

    // A bounded probe prevents a missing/floating module
    if (!probeRadio(sck, miso, mosi, cs, rst, busy, dio1)) {
        releaseRadioPins();
        spi_ = nullptr;
        return false;
    }

    // same injected SPI begin/end/remap flow as the other radio services
    spi_->end();
    delay(10);
    spi_->begin(sck, miso, mosi, cs);

    // Force SX126x-Arduino to use the same external SPI instance selected
    set_lora_spi_instance(spi_);

    hw_config config{};
    config.CHIP_TYPE = SX1262_CHIP;
    config.PIN_LORA_RESET = rst;
    config.PIN_LORA_NSS = cs;
    config.PIN_LORA_SCLK = sck;
    config.PIN_LORA_MISO = miso;
    config.PIN_LORA_DIO_1 = dio1;
    config.PIN_LORA_BUSY = busy;
    config.PIN_LORA_MOSI = mosi;
    config.RADIO_TXEN = -1;
    config.RADIO_RXEN = -1;
    config.USE_DIO2_ANT_SWITCH = true;
    config.USE_DIO3_TCXO = profile.tcxoVoltage > 0.0f;
    config.USE_DIO3_ANT_SWITCH = false;
    config.USE_LDO = false;
    config.USE_RXEN_ANT_PWR = false;
    config.TCXO_CTRL_VOLTAGE = tcxoControlVoltage(profile.tcxoVoltage);
    tcxoVoltage_ = profile.tcxoVoltage;

    lastError_ = static_cast<int16_t>(lora_hardware_init(config));
    if (lastError_ != 0) {
        // release the remapped SPI bus
        releaseSpiBus();
        return false;
    }
    #ifdef DEVICE_CARDPUTERADV
    initLoraCap();
    #endif

    hardwareInitialized_ = true;
    instance_ = this;
    events_ = {};
    events_.TxDone = onTxDone;
    events_.TxTimeout = onTxTimeout;
    events_.RxDone = onRxDone;
    events_.RxTimeout = onRxTimeout;
    events_.RxError = onRxError;
    events_.CadDone = onCadDone;

    Radio.Init(&events_);
    Radio.Sleep();

    const uint32_t frequencyHz = static_cast<uint32_t>(profile.frequency * 1000000.0f);
    if (!Radio.CheckRfFrequency(frequencyHz)) {
        lastError_ = -100;
        deinitRfModule();
        return false;
    }

    Radio.SetChannel(frequencyHz);
    currentFrequency_ = profile.frequency;
    initialized_ = true;

    if (!setModemProfile(profile)) {
        lastError_ = -101;
        deinitRfModule();
        return false;
    }

    resetStats();
    clearReceiveQueue();
    Radio.Sleep();
    lastError_ = 0;
    return true;
}

void LoRaService::deinitRfModule() {
    if (initialized_) {
        stopReceive();
        stopContinuousWave();
        Radio.Sleep();
    }

    if (hardwareInitialized_) {
    #ifdef DEVICE_CARDPUTERADV
        deInitLoraCap();
    #endif
        lora_hardware_uninit();
    }

    initialized_ = false;
    hardwareInitialized_ = false;
    receiving_ = false;
    continuousWave_ = false;
    currentFrequency_ = 0.0f;
    clearReceiveQueue();
    releaseReceiveQueue();
    releaseSpiBus();
    if (instance_ == this) instance_ = nullptr;
}

bool LoRaService::send(const uint8_t* data, size_t length) {
    if (!initialized_ || !data || length == 0 || length > MAX_PACKET_SIZE) {
        lastError_ = -110;
        return false;
    }

    stopReceive();
    clearReceiveQueue();
    txComplete_ = false;
    txSuccess_ = false;

    const uint32_t airTime = Radio.TimeOnAir(
        MODEM_LORA, static_cast<uint8_t>(length));
    const uint32_t timeoutMs = std::min<uint32_t>(
        30000, std::max<uint32_t>(2500, airTime + 2000));

    // Keep the driver's own timer aligned with our local wait
    Radio.SetTxConfig(MODEM_LORA, power_, 0, bandwidthIndex_,
                      spreadingFactor_, libraryCodingRate_, preambleLength_,
                      false, crc_, false, 0, invertIq_, timeoutMs);
    Radio.SetCustomSyncWord(syncWord_);
    Radio.SetMaxPayloadLength(MODEM_LORA, MAX_PACKET_SIZE);
    Radio.Send(const_cast<uint8_t*>(data), static_cast<uint8_t>(length));

    const uint32_t started = millis();
    while (!txComplete_ && millis() - started <= timeoutMs + 250) {
        delay(1);
    }

    Radio.Sleep();

    if (!txComplete_) {
        txErrors_ = txErrors_ + 1;
        lastError_ = -111;
        return false;
    }

    lastError_ = txSuccess_ ? 0 : -112;
    return txSuccess_;
}

bool LoRaService::startContinuousWave() {
    if (!initialized_) {
        lastError_ = -113;
        return false;
    }

    stopReceive();
    Radio.Standby();
    SX126xSetRfFrequency(static_cast<uint32_t>(currentFrequency_ * 1000000.0f));
    SX126xSetRfTxPower(power_);
    SX126xSetTxContinuousWave();
    continuousWave_ = true;
    lastError_ = 0;
    return true;
}

void LoRaService::stopContinuousWave() {
    if (!initialized_ || !continuousWave_) return;
    Radio.Sleep();
    continuousWave_ = false;
}

bool LoRaService::startReceive(bool boosted) {
    if (!initialized_) {
        lastError_ = RECEIVE_NOT_INITIALIZED;
        return false;
    }

    stopReceive();
    if (!ensureReceiveQueue()) {
        lastError_ = -114;
        return false;
    }
    clearReceiveQueue();
    rxErrorPending_ = false;

    // Reapply the full RX profile before every listening session
    Radio.SetCustomSyncWord(syncWord_);
    Radio.SetRxConfig(MODEM_LORA, bandwidthIndex_, spreadingFactor_,
                      libraryCodingRate_, 0, preambleLength_, 0, false, 0,
                      crc_, false, 0, invertIq_, true);
    Radio.SetMaxPayloadLength(MODEM_LORA, MAX_PACKET_SIZE);

    // Timeout 0 is intentional: the controller polls this continuous session
    // and stops it explicitly. No library timer is left running afterwards.
    if (boosted) Radio.RxBoosted(0);
    else Radio.Rx(0);

    receiving_ = true;
    lastError_ = 0;
    return true;
}

int16_t LoRaService::pollReceive(std::vector<uint8_t>& payload) {
    payload.clear();
    if (!initialized_) return RECEIVE_NOT_INITIALIZED;


    uint8_t localData[MAX_PACKET_SIZE]{};
    uint16_t localSize = 0;
    int16_t localRssi = 0;
    int8_t localSnr = 0;
    bool hasPacket = false;
    bool hasError = false;

    portENTER_CRITICAL(&rxMux_);
    if (rxQueue_ && rxQueueCount_ > 0) {
        const RxPacketSlot& slot = rxQueue_[rxQueueTail_];
        localSize = slot.size;
        localRssi = slot.rssi;
        localSnr = slot.snr;
        if (localSize > 0) {
            std::memcpy(localData, slot.data, localSize);
        }
        rxQueueTail_ = static_cast<uint8_t>(
            (rxQueueTail_ + 1) % RX_QUEUE_SIZE);
        rxQueueCount_ = rxQueueCount_ - 1;
        hasPacket = true;
    } else if (rxErrorPending_) {
        rxErrorPending_ = false;
        hasError = true;
    }
    portEXIT_CRITICAL(&rxMux_);

    if (hasError) return RECEIVE_ERROR;
    if (!hasPacket) return RECEIVE_TIMEOUT;

    payload.assign(localData, localData + localSize);
    lastPacketLength_ = localSize;
    lastRssi_ = static_cast<float>(localRssi);
    lastSnr_ = static_cast<float>(localSnr);
    return RECEIVE_OK;
}

void LoRaService::stopReceive() {
    if (!initialized_ || !receiving_) return;

    Radio.Sleep();
    receiving_ = false;
}

int16_t LoRaService::receive(std::vector<uint8_t>& payload, uint32_t timeoutMs,
                             bool boosted, bool countTimeout) {
    payload.clear();
    if (!initialized_) return RECEIVE_NOT_INITIALIZED;
    if (timeoutMs == 0) timeoutMs = 1;

    if (!startReceive(boosted)) return RECEIVE_NOT_INITIALIZED;

    const uint32_t started = millis();
    while (millis() - started < timeoutMs) {
        const int16_t result = pollReceive(payload);
        if (result == RECEIVE_OK || result == RECEIVE_ERROR) {
            stopReceive();
            return result;
        }
        delay(1);
    }

    stopReceive();
    if (countTimeout) rxTimeouts_ = rxTimeouts_ + 1;
    return RECEIVE_TIMEOUT;
}

bool LoRaService::setFrequency(float frequency) {
    if (!initialized_) return false;

    const uint32_t frequencyHz = static_cast<uint32_t>(frequency * 1000000.0f);
    if (!Radio.CheckRfFrequency(frequencyHz)) return false;

    stopReceive();
    Radio.Sleep();
    Radio.SetChannel(frequencyHz);
    currentFrequency_ = frequency;
    lastError_ = 0;
    return true;
}

bool LoRaService::setModemProfile(const LoRaRadioProfile& profile) {
    const uint8_t sf = profile.spreadingFactor;
    const uint8_t cr = profile.codingRate;
    const uint16_t bandwidth = profile.bandwidth;
    const bool supportedBandwidth =
        bandwidth == 125 || bandwidth == 250 || bandwidth == 500;
    if (!initialized_ || !supportedBandwidth ||
        sf < 6 || sf > 12 || cr < 5 || cr > 8) {
        lastError_ = -120;
        return false;
    }

    stopReceive();
    Radio.Sleep();

    bandwidthIndex_ = bandwidthIndex(profile.bandwidth);
    spreadingFactor_ = sf;
    libraryCodingRate_ = static_cast<uint8_t>(
        constrain(static_cast<int>(cr) - 4, 1, 4));
    power_ = profile.power;
    preambleLength_ = profile.preambleLength;
    syncWord_ = profile.syncWord;
    crc_ = profile.crc;
    invertIq_ = profile.invertIq;

    Radio.SetCustomSyncWord(syncWord_);
    Radio.SetTxConfig(MODEM_LORA, power_, 0, bandwidthIndex_,
                      spreadingFactor_, libraryCodingRate_, preambleLength_,
                      false, crc_, false, 0, invertIq_, 15000);
    Radio.SetRxConfig(MODEM_LORA, bandwidthIndex_, spreadingFactor_,
                      libraryCodingRate_, 0, preambleLength_, 0, false, 0,
                      crc_, false, 0, invertIq_, true);
    Radio.SetMaxPayloadLength(MODEM_LORA, MAX_PACKET_SIZE);

    clearReceiveQueue();
    Radio.Sleep();
    lastError_ = 0;
    return true;
}

LoRaRadioProfile LoRaService::getProfile() const {
    LoRaRadioProfile profile;
    profile.frequency = currentFrequency_;
    profile.bandwidth = bandwidthIndex_ == 2 ? 500 : (bandwidthIndex_ == 1 ? 250 : 125);
    profile.spreadingFactor = spreadingFactor_;
    profile.codingRate = static_cast<uint8_t>(libraryCodingRate_ + 4);
    profile.power = power_;
    profile.preambleLength = preambleLength_;
    profile.syncWord = syncWord_;
    profile.tcxoVoltage = tcxoVoltage_;
    profile.crc = crc_;
    profile.invertIq = invertIq_;
    return profile;
}

bool LoRaService::transmitFrame(const LoRaFrame& frame, bool& profileRestored) {
    profileRestored = false;
    if (!initialized_) return false;
    const LoRaRadioProfile original = getProfile();
    const bool applied = setFrequency(frame.profile.frequency) && setModemProfile(frame.profile);
    if (!applied) {
        setFrequency(original.frequency);
        profileRestored = setModemProfile(original);
        return false;
    }
    const bool sent = send(frame.payload.data(), frame.payload.size());
    profileRestored = setFrequency(original.frequency) && setModemProfile(original);
    return sent;
}

bool LoRaService::measureRssi(float frequency, uint32_t durationMs,
                              RssiStats& stats) {
    stats = {};
    if (!initialized_ || durationMs == 0 || !setFrequency(frequency)) {
        return false;
    }

    int16_t minimum = std::numeric_limits<int16_t>::max();
    int16_t maximum = std::numeric_limits<int16_t>::min();
    int32_t total = 0;
    uint32_t samples = 0;

    measuringRssi_ = true;
    if (!startReceive(false)) {
        measuringRssi_ = false;
        return false;
    }
    delay(2);

    const uint32_t started = millis();
    while (millis() - started < durationMs) {
        const int16_t rssi = Radio.Rssi(MODEM_LORA);
        if (rssi <= 0 && rssi >= -170) {
            minimum = std::min(minimum, rssi);
            maximum = std::max(maximum, rssi);
            total += rssi;
            samples++;
        }
        delay(5);
    }

    stopReceive();
    clearReceiveQueue();
    rxErrorPending_ = false;
    measuringRssi_ = false;
    if (samples == 0) return false;

    stats.minimum = minimum;
    stats.maximum = maximum;
    stats.average = static_cast<float>(total) / static_cast<float>(samples);
    stats.samples = samples;
    return true;
}

bool LoRaService::runCad(bool& detected, uint32_t timeoutMs) {
    detected = false;
    if (!initialized_) return false;

    stopReceive();
    cadComplete_ = false;
    cadDetected_ = false;

    Radio.Standby();
    Radio.SetCadParams(LORA_CAD_04_SYMBOL,
                       cadDetectionPeak(spreadingFactor_),
                       10, LORA_CAD_ONLY, 0);
    Radio.StartCad();

    const uint32_t started = millis();
    while (!cadComplete_ && millis() - started < timeoutMs) {
        delay(1);
    }

    Radio.Sleep();
    if (!cadComplete_) {
        lastError_ = -130;
        return false;
    }

    detected = cadDetected_;
    lastError_ = 0;
    return true;
}

uint32_t LoRaService::getTimeOnAir(size_t payloadLength) const {
    if (!initialized_ || payloadLength == 0 || payloadLength > MAX_PACKET_SIZE) {
        return 0;
    }
    return Radio.TimeOnAir(MODEM_LORA,
                           static_cast<uint8_t>(payloadLength));
}

void LoRaService::resetStats() {
    txPackets_ = 0;
    txErrors_ = 0;
    rxPackets_ = 0;
    rxTimeouts_ = 0;
    rxErrors_ = 0;
    rxDropped_ = 0;
    lastPacketLength_ = 0;
    lastRssi_ = 0.0f;
    lastSnr_ = 0.0f;
}

bool LoRaService::ensureReceiveQueue() {
    if (rxQueue_) return true;

    RxPacketSlot* queue = new RxPacketSlot[RX_QUEUE_SIZE];
    if (!queue) return false;

    portENTER_CRITICAL(&rxMux_);
    rxQueue_ = queue;
    rxQueueHead_ = 0;
    rxQueueTail_ = 0;
    rxQueueCount_ = 0;
    rxErrorPending_ = false;
    portEXIT_CRITICAL(&rxMux_);
    return true;
}

void LoRaService::clearReceiveQueue() {
    portENTER_CRITICAL(&rxMux_);
    rxQueueHead_ = 0;
    rxQueueTail_ = 0;
    rxQueueCount_ = 0;
    rxErrorPending_ = false;
    portEXIT_CRITICAL(&rxMux_);
}

void LoRaService::releaseReceiveQueue() {
    RxPacketSlot* queue = nullptr;
    portENTER_CRITICAL(&rxMux_);
    queue = rxQueue_;
    rxQueue_ = nullptr;
    rxQueueHead_ = 0;
    rxQueueTail_ = 0;
    rxQueueCount_ = 0;
    rxErrorPending_ = false;
    portEXIT_CRITICAL(&rxMux_);
    delete[] queue;
}


void LoRaService::onTxDone() {
    if (!instance_) return;
    instance_->txPackets_ = instance_->txPackets_ + 1;
    instance_->txSuccess_ = true;
    instance_->txComplete_ = true;
}

void LoRaService::onTxTimeout() {
    if (!instance_) return;
    instance_->txErrors_ = instance_->txErrors_ + 1;
    instance_->txSuccess_ = false;
    instance_->txComplete_ = true;
}

void LoRaService::onRxDone(uint8_t* payload, uint16_t size,
                           int16_t rssi, int8_t snr) {
    if (!instance_ || !payload || !instance_->rxQueue_) return;
    if (instance_->measuringRssi_) return;

    const uint16_t safeSize = std::min<uint16_t>(size, MAX_PACKET_SIZE);

    portENTER_CRITICAL(&instance_->rxMux_);
    if (instance_->rxQueueCount_ >= RX_QUEUE_SIZE) {
        instance_->rxQueueTail_ = static_cast<uint8_t>(
            (instance_->rxQueueTail_ + 1) % RX_QUEUE_SIZE);
        instance_->rxQueueCount_ = instance_->rxQueueCount_ - 1;
        instance_->rxDropped_ = instance_->rxDropped_ + 1;
    }

    RxPacketSlot& slot = instance_->rxQueue_[instance_->rxQueueHead_];
    slot.size = safeSize;
    slot.rssi = rssi;
    slot.snr = snr;
    if (safeSize > 0) std::memcpy(slot.data, payload, safeSize);

    instance_->rxQueueHead_ = static_cast<uint8_t>(
        (instance_->rxQueueHead_ + 1) % RX_QUEUE_SIZE);
    instance_->rxQueueCount_ = instance_->rxQueueCount_ + 1;
    portEXIT_CRITICAL(&instance_->rxMux_);

    instance_->rxPackets_ = instance_->rxPackets_ + 1;
}

void LoRaService::onRxTimeout() {
    if (!instance_) return;
    instance_->rxTimeouts_ = instance_->rxTimeouts_ + 1;
}

void LoRaService::onRxError() {
    if (!instance_ || instance_->measuringRssi_) return;
    instance_->rxErrors_ = instance_->rxErrors_ + 1;
    portENTER_CRITICAL(&instance_->rxMux_);
    instance_->rxErrorPending_ = true;
    portEXIT_CRITICAL(&instance_->rxMux_);
}

void LoRaService::onCadDone(bool detected) {
    if (!instance_) return;
    instance_->cadDetected_ = detected;
    instance_->cadComplete_ = true;
}
