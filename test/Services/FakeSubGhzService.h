#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Interfaces/ISubGhzService.h"

class FakeSubGhzService final : public ISubGhzService {
public:
    struct ConfigureCall {
        uint8_t sck = 0;
        uint8_t miso = 0;
        uint8_t mosi = 0;
        uint8_t ss = 0;
        uint8_t gdo0 = 0;
        float mhz = 0.0f;
        int paDbm = 0;
        bool useCardputerAdvCap = false;
    };

    struct SendRawFrameCall {
        int pin = 0;
        std::vector<rmt_symbol_word_t> items;
        uint32_t tickPerUs = 0;
    };

    bool configureResult = true;
    bool startRawSnifferResult = true;
    bool startTxBitBangResult = true;
    bool stopTxBitBangResult = true;
    bool sendRawFrameResult = true;
    bool sendRandomBurstResult = true;
    bool sendRawPulseResult = true;
    bool sendRcSwitchResult = true;
    bool sendPrincetonResult = true;
    bool sendBinRawResult = true;
    bool sendTimingsOokResult = true;
    bool sendRawTimingsResult = true;
    bool sendTimingsRawSignedResult = true;
    bool sendResult = true;
    bool applyDefaultProfileResult = true;
    bool applySniffProfileResult = true;
    bool applyRawSendProfileResult = true;
    bool applyPresetByNameResult = true;
    bool applyScanProfileResult = true;
    uint32_t rxTickPerUs = 1;

    std::vector<std::string> supportedBands = {"All", "EU 433"};
    std::map<std::string, std::vector<float>> supportedFreqs = {
        {"All", {315.0f, 433.92f}},
        {"EU 433", {433.42f, 433.92f}}
    };

    std::vector<ConfigureCall> configureCalls;
    std::vector<float> tuneCalls;
    std::deque<int> rssiSamples;
    std::vector<std::string> scanBands;
    std::vector<int> rawSnifferPins;
    std::deque<std::pair<std::string, size_t>> rawPulses;
    std::deque<std::vector<rmt_symbol_word_t>> rawSymbolsUntilFrames;
    std::deque<std::vector<rmt_symbol_word_t>> rawChunks;
    std::deque<std::vector<rmt_symbol_word_t>> rawFrames;
    std::vector<SendRawFrameCall> sendRawFrameCalls;
    std::vector<int> randomBurstPins;
    std::vector<int> rawPulsePins;
    std::vector<int> rawPulseDurations;
    std::vector<SubGhzFileCommand> sentCommands;
    std::vector<float> defaultProfiles;
    std::vector<float> sniffProfiles;
    std::vector<float> rawSendProfiles;
    std::vector<std::string> presetNames;
    std::vector<float> presetFrequencies;
    std::vector<float> scanProfileDataRates;
    uint32_t stopRawSnifferCalls = 0;
    uint32_t startTxBitBangCalls = 0;
    uint32_t stopTxBitBangCalls = 0;
    uint32_t releaseSnifferResourcesCalls = 0;
    uint32_t deinitRfModuleCalls = 0;

    bool configure(SPIClass&,
                   uint8_t sck,
                   uint8_t miso,
                   uint8_t mosi,
                   uint8_t ss,
                   uint8_t gdo0,
                   float mhz = 433.92f,
                   int paDbm = 10,
                   bool useCardputerAdvCap = false) override {
        configureCalls.push_back({sck, miso, mosi, ss, gdo0, mhz, paDbm, useCardputerAdvCap});
        return configureResult;
    }

    void tune(float mhz) override { tuneCalls.push_back(mhz); }

    int measurePeakRssi(uint32_t) override {
        if (rssiSamples.empty()) return -90;
        const int value = rssiSamples.front();
        rssiSamples.pop_front();
        return value;
    }

    std::vector<std::string> getSupportedBand() const override { return supportedBands; }

    std::vector<float> getSupportedFreq(const std::string& band) const override {
        auto it = supportedFreqs.find(band);
        return it == supportedFreqs.end() ? std::vector<float>{} : it->second;
    }

    void setScanBand(const std::string& bandName) override { scanBands.push_back(bandName); }
    uint32_t getRxTickPerUs() const override { return rxTickPerUs; }

    bool startRawSniffer(int pin) override {
        rawSnifferPins.push_back(pin);
        return startRawSnifferResult;
    }

    std::pair<std::string, size_t> readRawPulses() override {
        if (rawPulses.empty()) return {"", 0};
        auto value = rawPulses.front();
        rawPulses.pop_front();
        return value;
    }

    std::vector<rmt_symbol_word_t> readRawSymbolsUntil(size_t, uint32_t) override {
        if (rawSymbolsUntilFrames.empty()) return {};
        auto value = rawSymbolsUntilFrames.front();
        rawSymbolsUntilFrames.pop_front();
        return value;
    }

    std::vector<rmt_symbol_word_t> readRawChunk() override {
        if (rawChunks.empty()) return {};
        auto value = rawChunks.front();
        rawChunks.pop_front();
        return value;
    }

    std::vector<rmt_symbol_word_t> readRawFrame() override {
        if (rawFrames.empty()) return {};
        auto value = rawFrames.front();
        rawFrames.pop_front();
        return value;
    }

    void stopRawSniffer() override { ++stopRawSnifferCalls; }

    bool startTxBitBang() override {
        ++startTxBitBangCalls;
        return startTxBitBangResult;
    }

    bool stopTxBitBang() override {
        ++stopTxBitBangCalls;
        return stopTxBitBangResult;
    }

    bool sendRawFrame(int pin,
                      const std::vector<rmt_symbol_word_t>& items,
                      uint32_t tick_per_us = 1) override {
        sendRawFrameCalls.push_back({pin, items, tick_per_us});
        return sendRawFrameResult;
    }

    bool sendRandomBurst(int pin) override {
        randomBurstPins.push_back(pin);
        return sendRandomBurstResult;
    }

    bool sendRawPulse(int pin, int duration) override {
        rawPulsePins.push_back(pin);
        rawPulseDurations.push_back(duration);
        return sendRawPulseResult;
    }

    bool sendRcSwitch_(uint64_t, uint16_t, int, int, int) override { return sendRcSwitchResult; }
    bool sendPrinceton_(uint64_t, uint16_t, int) override { return sendPrincetonResult; }
    bool sendBinRaw_(const std::vector<uint8_t>&, int, int, bool = true, bool = false) override { return sendBinRawResult; }
    bool sendTimingsOOK_(const std::vector<int32_t>&) override { return sendTimingsOokResult; }
    bool sendRawTimings(const std::vector<int32_t>&) override { return sendRawTimingsResult; }
    bool sendTimingsRawSigned_(const std::vector<int32_t>&) override { return sendTimingsRawSignedResult; }

    bool send(const SubGhzFileCommand& cmd) override {
        sentCommands.push_back(cmd);
        return sendResult;
    }

    bool applyDefaultProfile(float mhz = 433.92f) override {
        defaultProfiles.push_back(mhz);
        return applyDefaultProfileResult;
    }

    bool applySniffProfile(float mhz) override {
        sniffProfiles.push_back(mhz);
        return applySniffProfileResult;
    }

    bool applyRawSendProfile(float mhz) override {
        rawSendProfiles.push_back(mhz);
        return applyRawSendProfileResult;
    }

    bool applyPresetByName(const std::string& name, float mhz) override {
        presetNames.push_back(name);
        presetFrequencies.push_back(mhz);
        return applyPresetByNameResult;
    }

    bool applyScanProfile(float dataRateKbps = 4.8f,
                          float = 200.0f,
                          uint8_t = 2,
                          bool = true) override {
        scanProfileDataRates.push_back(dataRateKbps);
        return applyScanProfileResult;
    }

    void releaseSnifferResources() override { ++releaseSnifferResourcesCalls; }
    void deinitRfModule() override { ++deinitRfModuleCalls; }
};

