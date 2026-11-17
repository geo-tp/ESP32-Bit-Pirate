#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SX126x-Arduino.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Interfaces/ILoRaService.h"
#ifdef DEVICE_CARDPUTERADV
#include <Wire.h>
#endif

class LoRaService : public ILoRaService {
public:
    bool configure(SPIClass& spi, uint8_t sck, uint8_t miso, uint8_t mosi,
                   uint8_t cs, uint8_t rst, uint8_t busy, uint8_t dio1,
                   const LoRaRadioProfile& profile) override;

    void deinitRfModule() override;

    bool send(const uint8_t* data, size_t length) override;
    bool startContinuousWave() override;
    void stopContinuousWave() override;

    bool startReceive(bool boosted = true) override;
    int16_t pollReceive(std::vector<uint8_t>& payload) override;
    void stopReceive() override;
    bool isReceiving() const override { return receiving_; }
    int16_t receive(std::vector<uint8_t>& payload, uint32_t timeoutMs,
                    bool boosted = true, bool countTimeout = true) override;

    bool setFrequency(float frequency) override;
    bool setModemProfile(const LoRaRadioProfile& profile) override;
    bool transmitFrame(const LoRaFrame& frame, bool& profileRestored) override;
    LoRaRadioProfile getProfile() const override;
    bool measureRssi(float frequency, uint32_t durationMs, RssiStats& stats) override;
    bool runCad(bool& detected, uint32_t timeoutMs = 250) override;
    uint32_t getTimeOnAir(size_t payloadLength) const override;

    bool isInitialized() const override { return initialized_; }
    float getCurrentFrequency() const override { return currentFrequency_; }
    float getRssi() const override { return lastRssi_; }
    float getSnr() const override { return lastSnr_; }
    size_t getLastPacketLength() const override { return lastPacketLength_; }
    int16_t getLastError() const override { return lastError_; }

    uint32_t getTxPackets() const override { return txPackets_; }
    uint32_t getTxErrors() const override { return txErrors_; }
    uint32_t getRxPackets() const override { return rxPackets_; }
    uint32_t getRxTimeouts() const override { return rxTimeouts_; }
    uint32_t getRxErrors() const override { return rxErrors_; }
    uint32_t getRxDropped() const override { return rxDropped_; }
    void resetStats() override;

private:
    static constexpr uint8_t RX_QUEUE_SIZE = 8;
    static constexpr uint16_t MAX_PACKET_SIZE = 255;

    struct RxPacketSlot {
        uint8_t data[MAX_PACKET_SIZE]{};
        uint16_t size = 0;
        int16_t rssi = 0;
        int8_t snr = 0;
    };

    static LoRaService* instance_;
    static void onTxDone();
    static void onTxTimeout();
    static void onRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);
    static void onRxTimeout();
    static void onRxError();
    static void onCadDone(bool detected);

    static uint32_t bandwidthIndex(float bandwidth);
    static RadioTcxoCtrlVoltage_t tcxoControlVoltage(float voltage);
    static uint8_t cadDetectionPeak(uint8_t spreadingFactor);

    bool probeRadio(uint8_t sck, uint8_t miso, uint8_t mosi,
                    uint8_t cs, uint8_t rst, uint8_t busy, uint8_t dio1);
    static uint8_t probeTransfer(uint8_t value, uint8_t sck,
                                 uint8_t miso, uint8_t mosi);
    static bool waitBusyLow(uint8_t busy, uint32_t timeoutMs);
    void releaseSpiBus();
    void releaseRadioPins();

    #ifdef DEVICE_CARDPUTERADV
    TwoWire *findLoraCapBus();
    bool pi4ioWrite(TwoWire *bus, uint8_t reg, uint8_t val);
    void initLoraCap();
    void deInitLoraCap();
    #endif

    bool ensureReceiveQueue();
    void clearReceiveQueue();
    void releaseReceiveQueue();

    RadioEvents_t events_{};

    SPIClass* spi_ = nullptr;
    uint8_t sckPin_ = 0;
    uint8_t misoPin_ = 0;
    uint8_t mosiPin_ = 0;
    uint8_t csPin_ = 0;
    uint8_t rstPin_ = 0;
    uint8_t busyPin_ = 0;
    uint8_t dio1Pin_ = 0;
    bool hardwareInitialized_ = false;
    bool pinsAssigned_ = false;

    volatile bool txComplete_ = false;
    volatile bool txSuccess_ = false;
    volatile bool receiving_ = false;
    volatile bool rxErrorPending_ = false;
    volatile bool cadComplete_ = false;
    volatile bool cadDetected_ = false;
    volatile bool measuringRssi_ = false;
    volatile bool continuousWave_ = false;

    // Allocated lazily on first RX 
    RxPacketSlot* rxQueue_ = nullptr;
    volatile uint8_t rxQueueHead_ = 0;
    volatile uint8_t rxQueueTail_ = 0;
    volatile uint8_t rxQueueCount_ = 0;
    portMUX_TYPE rxMux_ = portMUX_INITIALIZER_UNLOCKED;

    float lastRssi_ = 0.0f;
    float lastSnr_ = 0.0f;
    size_t lastPacketLength_ = 0;
    int16_t lastError_ = 0;
    float currentFrequency_ = 0.0f;

    uint32_t bandwidthIndex_ = 0;
    uint8_t spreadingFactor_ = 9;
    uint8_t libraryCodingRate_ = 3;
    int8_t power_ = 14;
    uint16_t preambleLength_ = 8;
    uint16_t syncWord_ = 0x1424;
    float tcxoVoltage_ = 1.8f;
    bool crc_ = true;
    bool invertIq_ = false;
    bool initialized_ = false;

    volatile uint32_t txPackets_ = 0;
    volatile uint32_t txErrors_ = 0;
    volatile uint32_t rxPackets_ = 0;
    volatile uint32_t rxTimeouts_ = 0;
    volatile uint32_t rxErrors_ = 0;
    volatile uint32_t rxDropped_ = 0;
};
