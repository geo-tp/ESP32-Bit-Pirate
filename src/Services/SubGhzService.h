#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "ELECHOUSE_CC1101_SRC_DRV.h"
#include "Data/SugGhzFreqs.h"
#include "Interfaces/ISubGhzService.h"
#include "Models/SubghzFileCommand.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"

#define RMT_RX_CHANNEL RMT_CHANNEL_6
#define RMT_TX_CHANNEL RMT_CHANNEL_5
#define RMT_CLK_DIV 80
#define RMT_1US_TICKS (80000000 / RMT_CLK_DIV / 1000000)
#define RMT_1MS_TICKS (RMT_1US_TICKS * 1000)
#define RMT_BUFFER_SIZE 8192

#define TEMBED_CC1101_SW0 48
#define TEMBED_CC1101_SW1 47

class SubGhzService : public ISubGhzService {
public:
    // Configure CC1101
    bool configure(SPIClass& spi, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t ss, uint8_t gdo0,
                   float mhz = 433.92f, // default 433.92mhz
                   int paDbm = 10, // TX power, max 12
                   bool useCardputerAdvCap = false);
    
    // Base
    void tune(float mhz);
    int measurePeakRssi(uint32_t holdMs);
    std::vector<std::string> getSupportedBand() const;
    std::vector<float> getSupportedFreq(const std::string& band) const;
    void setScanBand(const std::string& bandName);
    uint32_t getRxTickPerUs() const;

    // RMT raw sniffer
    bool startRawSniffer(int pin);
    std::pair<std::string, size_t> readRawPulses();
    std::vector<rmt_symbol_word_t> readRawSymbolsUntil(size_t numSamples, uint32_t timeoutMs);
    std::vector<rmt_symbol_word_t> readRawChunk();
    std::vector<rmt_symbol_word_t> readRawFrame();
    void stopRawSniffer();

    // Raw send
    bool startTxBitBang();
    bool stopTxBitBang();
    bool sendRawFrame(int pin,
                      const std::vector<rmt_symbol_word_t>& items,
                      uint32_t tick_per_us = RMT_1US_TICKS);
    bool sendRandomBurst(int pin);
    bool sendRawPulse(int pin, int duration);
    bool sendRcSwitch_(uint64_t key, uint16_t bits, int te_us, int proto, int repeat);
    bool sendPrinceton_(uint64_t key, uint16_t bits, int te_us);
    bool sendBinRaw_(const std::vector<uint8_t>& bytes, int te_us, int bits, bool msb_first = true, bool invert = false);
    bool sendTimingsOOK_(const std::vector<int32_t>& timings); 
    bool sendRawTimings(const std::vector<int32_t>& timings);
    bool sendTimingsRawSigned_(const std::vector<int32_t>& timings);
    bool send(const SubGhzFileCommand& cmd);

    // Profiles
    bool applyDefaultProfile(float mhz = 433.92f);
    bool applySniffProfile(float mhz);
    bool applyRawSendProfile(float mhz);
    bool applyPresetByName(const std::string& name, float mhz);
    bool applyScanProfile(float dataRateKbps = 4.8f,
                          float rxBwKhz      = 200.0f,
                          uint8_t modulation = 2,    // 2 = OOK/ASK
                          bool packetMode    = true);
    void releaseSnifferResources();
    void deinitRfModule();


private:
    bool    isConfigured_ = false;
    uint8_t sck_ = 0, miso_ = 0, mosi_ = 0, ss_ = 0;
    uint8_t gdo0_ = 0;
    float   mhz_ = 433.92f;
    int     paDbm_ = 10;
    bool    ccMode_ = false;
    bool    useCardputerAdvCap_ = false;
    SubGhzScanBand scanBand_ = SubGhzScanBand::Band387_464;
    RingbufHandle_t rb_ = nullptr;
    uint8_t rfSw0_ = TEMBED_CC1101_SW0;
    uint8_t rfSw1_ = TEMBED_CC1101_SW1;
    uint8_t rfSel_ = 0xFF; // no RF path selected yet

    rmt_channel_handle_t rx_chan_ = nullptr;
    std::vector<rmt_symbol_word_t> rx_buf_;
    std::vector<rmt_symbol_word_t> rx_ring_;
    size_t rx_ring_head_ = 0;
    size_t rx_ring_tail_ = 0;
    size_t last_symbols_ = 0;
    bool rx_done_ = false;
    uint32_t rx_resolution_hz_ = 0;
    uint32_t rx_tick_per_us_   = 0;
    TaskHandle_t rx_task_ = nullptr;
    bool rx_task_running_ = false;
    portMUX_TYPE rx_ring_mux_ = portMUX_INITIALIZER_UNLOCKED;
    
    static constexpr size_t kRxDmaSymbols = 1024;
    static constexpr size_t kRxRingSymbols = 2048;
    static constexpr size_t kRxChunkSymbols = 256;
    static constexpr uint32_t kRxTaskStackWords = 3072;

    // RF switch selection for T-Embed and Cardputer ADV Cap CC1101
    void initTembed();
    void selectRfPathFor(float mhz, bool force = false);

    // Presets
    static bool IRAM_ATTR on_rx_done(rmt_channel_handle_t,
                                const rmt_rx_done_event_data_t* edata,
                                void* user);
    static void rxTaskEntry(void* arg);
    void rxTaskLoop();
    bool restartRxReceive();
    void pushRxSymbols(const rmt_symbol_word_t* src, size_t count);
    size_t popRxSymbols(std::vector<rmt_symbol_word_t>& out, size_t maxSymbols);
};
