#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "driver/rmt_types.h"

#include "Models/SubghzFileCommand.h"

class SPIClass;

class ISubGhzService {
public:
    virtual ~ISubGhzService() = default;

    virtual bool configure(SPIClass& spi,
                           uint8_t sck,
                           uint8_t miso,
                           uint8_t mosi,
                           uint8_t ss,
                           uint8_t gdo0,
                           float mhz = 433.92f,
                           int paDbm = 10,
                           bool useCardputerAdvCap = false) = 0;

    virtual void tune(float mhz) = 0;
    virtual int measurePeakRssi(uint32_t holdMs) = 0;
    virtual std::vector<std::string> getSupportedBand() const = 0;
    virtual std::vector<float> getSupportedFreq(const std::string& band) const = 0;
    virtual void setScanBand(const std::string& bandName) = 0;
    virtual uint32_t getRxTickPerUs() const = 0;

    virtual bool startRawSniffer(int pin) = 0;
    virtual std::pair<std::string, size_t> readRawPulses() = 0;
    virtual std::vector<rmt_symbol_word_t> readRawSymbolsUntil(size_t numSamples, uint32_t timeoutMs) = 0;
    virtual std::vector<rmt_symbol_word_t> readRawChunk() = 0;
    virtual std::vector<rmt_symbol_word_t> readRawFrame() = 0;
    virtual void stopRawSniffer() = 0;

    virtual bool startTxBitBang() = 0;
    virtual bool stopTxBitBang() = 0;
    virtual bool sendRawFrame(int pin,
                              const std::vector<rmt_symbol_word_t>& items,
                              uint32_t tick_per_us = 1) = 0;
    virtual bool sendRandomBurst(int pin) = 0;
    virtual bool sendRawPulse(int pin, int duration) = 0;
    virtual bool sendRcSwitch_(uint64_t key, uint16_t bits, int te_us, int proto, int repeat) = 0;
    virtual bool sendPrinceton_(uint64_t key, uint16_t bits, int te_us) = 0;
    virtual bool sendBinRaw_(const std::vector<uint8_t>& bytes,
                             int te_us,
                             int bits,
                             bool msb_first = true,
                             bool invert = false) = 0;
    virtual bool sendTimingsOOK_(const std::vector<int32_t>& timings) = 0;
    virtual bool sendRawTimings(const std::vector<int32_t>& timings) = 0;
    virtual bool sendTimingsRawSigned_(const std::vector<int32_t>& timings) = 0;
    virtual bool send(const SubGhzFileCommand& cmd) = 0;

    virtual bool applyDefaultProfile(float mhz = 433.92f) = 0;
    virtual bool applySniffProfile(float mhz) = 0;
    virtual bool applyRawSendProfile(float mhz) = 0;
    virtual bool applyPresetByName(const std::string& name, float mhz) = 0;
    virtual bool applyScanProfile(float dataRateKbps = 4.8f,
                                  float rxBwKhz = 200.0f,
                                  uint8_t modulation = 2,
                                  bool packetMode = true) = 0;
    virtual void releaseSnifferResources() = 0;
    virtual void deinitRfModule() = 0;
};
