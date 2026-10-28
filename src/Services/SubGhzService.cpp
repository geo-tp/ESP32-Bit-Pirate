#include "SubGhzService.h"
#include <sstream>

// Base

bool SubGhzService::configure(SPIClass& spi, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t ss,
                              uint8_t gdo0, float mhz, int paDbm, bool useCardputerAdvCap)
{
    // Value assignment
    sck_ = sck; 
    miso_ = miso; 
    mosi_ = mosi; 
    ss_ = ss;
    gdo0_ = gdo0;
    mhz_ = mhz;
    paDbm_ = paDbm;
    ccMode_ = true;
    rfSel_ = 0xFF; // Re-apply the RF path after each CC1101 initialization

    #ifdef DEVICE_CARDPUTERADV
    useCardputerAdvCap_ = useCardputerAdvCap;
    #else
    (void)useCardputerAdvCap;
    useCardputerAdvCap_ = false;
    #endif

    #ifdef DEVICE_TEMBEDS3CC1101

    initTembed();
    
    #endif

    SPIClass* activeSpi = &spi;

    // SPI
    activeSpi->end();
    delay(10);
    activeSpi->begin(sck_, miso_, mosi_, ss_);
    
    // Initialize CC1101
    ELECHOUSE_cc1101.setSPIinstance(activeSpi);
    ELECHOUSE_cc1101.setBeginEndLogic(false);
    ELECHOUSE_cc1101.setSpiPin(sck_, miso_, mosi_, ss_);
    ELECHOUSE_cc1101.setGDO0(gdo0_);
    if (!ELECHOUSE_cc1101.Init()) {
        isConfigured_ = false;
        return false;
    }

    // Allow profile setup to run immediately after a successful init.
    isConfigured_ = true;
    if (!applyDefaultProfile(mhz_)) {
        isConfigured_ = false;
        return false;
    }

    // Check if CC1101 is configured
    delay(5);
    isConfigured_ = ELECHOUSE_cc1101.getCC1101();
    if (!isConfigured_) {
        return false;
    }

    ELECHOUSE_cc1101.setPA(paDbm_);
    ELECHOUSE_cc1101.SetRx(mhz_);
    selectRfPathFor(mhz_);

    return isConfigured_;
}

void SubGhzService::tune(float mhz)
{
    if (!isConfigured_) return;
    mhz_ = mhz;
    ELECHOUSE_cc1101.SetRx(mhz_);
    selectRfPathFor(mhz_);
}

int SubGhzService::measurePeakRssi(uint32_t holdMs)
{
    if (!isConfigured_) return -127;
    if (holdMs < 1) holdMs = 1;

    const unsigned long t0 = millis();
    int peak = -127;
    while (millis() - t0 < holdMs) {
        int r = ELECHOUSE_cc1101.getRssi();
        if (r > peak) peak = r;
        delay(1);
    }
    return peak;
}

std::vector<std::string> SubGhzService::getSupportedBand() const {
    return std::vector<std::string>(std::begin(kSubGhzScanBandNames), std::end(kSubGhzScanBandNames));
}

std::vector<float> SubGhzService::getSupportedFreq(const std::string& band) const {
    std::vector<float> out;

    int bandIdx = -1;
    for (int i = 0; i < 4; ++i) {
        if (band == kSubGhzScanBandNames[i]) {
            bandIdx = i;
            break;
        }
    }

    if (bandIdx < 0) {
        return out;
    }

    int start = kSubGhzRangeLimits[bandIdx][0];
    int end   = kSubGhzRangeLimits[bandIdx][1];

    out.reserve(static_cast<size_t>(end - start + 1));
    for (int i = start; i <= end; ++i) {
        out.push_back(kSubGhzFreqList[i]);
    }

    return out;
}

void SubGhzService::setScanBand(const std::string& s) {
    for (int i = 0; i < 4; ++i) {
        if (s == kSubGhzScanBandNames[i]) {
            scanBand_ = static_cast<SubGhzScanBand>(i);
            return;
        }
    }

    char* end = nullptr;
    long idx = std::strtol(s.c_str(), &end, 10);
    if (end != s.c_str()) {
        scanBand_ = static_cast<SubGhzScanBand>(idx);
    }
}

uint32_t SubGhzService::getRxTickPerUs() const {
    return rx_tick_per_us_;
}

// Raw sniffer

bool IRAM_ATTR SubGhzService::on_rx_done(rmt_channel_handle_t,
                                        const rmt_rx_done_event_data_t* edata,
                                        void* user) {
    auto* self = static_cast<SubGhzService*>(user);
    self->last_symbols_ = edata->num_symbols;
    self->rx_done_ = true;
    return false;
}

bool SubGhzService::startRawSniffer(int pin) {
    stopRawSniffer();

    // --- RX channel config (new driver)
    rmt_rx_channel_config_t cfg{};
    cfg.gpio_num        = (gpio_num_t)pin;
    cfg.clk_src         = RMT_CLK_SRC_DEFAULT;
    cfg.resolution_hz   = 1000000;        // 1 tick = 1 us
    cfg.flags.invert_in = false;
    cfg.flags.with_dma  = true;
    cfg.mem_block_symbols = 256;            // simple & safe (pair)

    rx_resolution_hz_ = cfg.resolution_hz;
    rx_tick_per_us_   = (rx_resolution_hz_ / 1000000U);
    if (rx_tick_per_us_ == 0) rx_tick_per_us_ = 1;

    esp_err_t err = rmt_new_rx_channel(&cfg, &rx_chan_);
    if (err != ESP_OK || !rx_chan_) {
        rx_chan_ = nullptr;
        return false;
    }

    // --- callbacks
    rmt_rx_event_callbacks_t cbs{};
    cbs.on_recv_done = &on_rx_done;
    err = rmt_rx_register_event_callbacks(rx_chan_, &cbs, this);
    if (err != ESP_OK) {
        stopRawSniffer();
        return false;
    }

    // --- enable channel
    err = rmt_enable(rx_chan_);
    if (err != ESP_OK) {
        stopRawSniffer();
        return false;
    }

    // --- buffers
    rx_buf_.assign(kRxDmaSymbols, {});
    rx_ring_.assign(kRxRingSymbols, {});
    rx_ring_head_ = 0;
    rx_ring_tail_ = 0;

    if (rx_buf_.empty() || rx_ring_.empty()) {
        stopRawSniffer();
        return false;
    }

    // --- receive config
    rmt_receive_config_t rcfg{};
    rcfg.signal_range_min_ns = 1'000;        // 1 us glitch filter
    rcfg.signal_range_max_ns = 20'000'000;   // 20 ms max same-level
    rcfg.flags.en_partial_rx = 1;            // allow piece-by-piece for long streams

    rx_done_ = false;
    last_symbols_ = 0;

    err = rmt_receive(rx_chan_,
                      rx_buf_.data(),
                      rx_buf_.size() * sizeof(rmt_symbol_word_t),
                      &rcfg);

    if (err != ESP_OK) {
        stopRawSniffer();
        return false;
    }

    rx_task_running_ = true;
    BaseType_t taskOk = xTaskCreatePinnedToCore(
        &SubGhzService::rxTaskEntry,
        "subghz_rx",
        kRxTaskStackWords,
        this,
        2,
        &rx_task_,
        tskNO_AFFINITY
    );
    if (taskOk != pdPASS || rx_task_ == nullptr) {
        stopRawSniffer();
        return false;
    }

    return true;
}

void SubGhzService::stopRawSniffer() {
    rx_task_running_ = false;
    if (rx_task_ != nullptr) {
        const unsigned long waitStart = millis();
        while (rx_task_ != nullptr && (millis() - waitStart) < 50) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        if (rx_task_ != nullptr) {
            vTaskDelete(rx_task_);
            rx_task_ = nullptr;
        }
    }

    if (rx_chan_) {
        rmt_disable(rx_chan_);
        rmt_del_channel(rx_chan_);
        rx_chan_ = nullptr;
    }

    std::vector<rmt_symbol_word_t>().swap(rx_buf_);
    std::vector<rmt_symbol_word_t>().swap(rx_ring_);
    rx_ring_head_ = 0;
    rx_ring_tail_ = 0;
    rx_done_ = false;
    last_symbols_ = 0;
}

std::vector<rmt_symbol_word_t> SubGhzService::readRawChunk() {
    std::vector<rmt_symbol_word_t> out;
    if (!rx_chan_) return out;
    out.reserve(kRxChunkSymbols);
    (void)popRxSymbols(out, kRxChunkSymbols);

    return out;
}

std::vector<rmt_symbol_word_t> SubGhzService::readRawFrame() {
    const size_t MIN_SYMS = 16;

    std::vector<rmt_symbol_word_t> out;
    if (!rx_chan_) return out;

    std::vector<rmt_symbol_word_t> snap;
    snap.reserve(kRxDmaSymbols);
    (void)popRxSymbols(snap, kRxDmaSymbols);
    if (snap.size() < MIN_SYMS) {
        return out;
    }

    const uint32_t GAP_US = 2000;

    auto isGap = [&](const rmt_symbol_word_t& s) -> bool {
        if (s.duration0 && s.duration0 >= GAP_US) return true;
        if (s.duration1 && s.duration1 >= GAP_US) return true;
        return false;
    };

    // find gap
    size_t gapIdx = snap.size();
    for (size_t i = 0; i < snap.size(); ++i) {
        if (isGap(snap[i])) { gapIdx = i; break; }
    }
    if (gapIdx == snap.size()) return out;

    // frame is after gap
    size_t start = gapIdx + 1;
    size_t end   = snap.size();
    for (size_t i = start; i < snap.size(); ++i) {
        if (isGap(snap[i])) { end = i; break; }
    }

    if (end <= start || (end - start) < MIN_SYMS) return out;

    out.insert(out.end(), snap.begin() + start, snap.begin() + end);
    return out;
}

std::pair<std::string, size_t> SubGhzService::readRawPulses() {
    std::vector<rmt_symbol_word_t> chunk;
    chunk.reserve(kRxChunkSymbols);
    const size_t n = popRxSymbols(chunk, kRxChunkSymbols);
    if (n == 0) return {"", 0};

    std::ostringstream oss;
    oss << "[raw " << n << " symbols | freq=" << mhz_ << " MHz]\r\n";

    int col = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto& s = chunk[i];
        oss << (s.level0 ? 'H' : 'L') << ":" << s.duration0
            << " | "
            << (s.level1 ? 'H' : 'L') << ":" << s.duration1
            << "   ";
        if (++col % 4 == 0) oss << "\r\n";
    }
    oss << "\r\n";

    return {oss.str(), n};
}

std::vector<rmt_symbol_word_t> SubGhzService::readRawSymbolsUntil(size_t numSamples, uint32_t timeoutMs)
{
    std::vector<rmt_symbol_word_t> out;

    if (!rx_chan_ || numSamples == 0)
        return out;

    if (timeoutMs == 0)
        timeoutMs = 1;

    out.reserve(numSamples);

    const unsigned long t0 = millis();

    while (out.size() < numSamples && (millis() - t0) < timeoutMs)
    {
        const size_t canTake = numSamples - out.size();
        const size_t took = popRxSymbols(out, canTake);
        if (took == 0) {
            delay(1);
        }
    }

    return out;
}

void SubGhzService::rxTaskEntry(void* arg) {
    auto* self = static_cast<SubGhzService*>(arg);
    self->rxTaskLoop();
    self->rx_task_ = nullptr;
    vTaskDelete(nullptr);
}

void SubGhzService::rxTaskLoop() {
    while (rx_task_running_) {
        if (!rx_done_) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        size_t n = (size_t)last_symbols_;
        if (n > rx_buf_.size()) n = rx_buf_.size();
        if (n > 0) {
            pushRxSymbols(rx_buf_.data(), n);
        }

        rx_done_ = false;
        last_symbols_ = 0;
        (void)restartRxReceive();
    }
}

bool SubGhzService::restartRxReceive() {
    if (!rx_chan_ || rx_buf_.empty()) return false;

    rmt_receive_config_t rcfg{};
    rcfg.signal_range_min_ns = 1000;
    rcfg.signal_range_max_ns = 20'000'000;
    rcfg.flags.en_partial_rx = 1;

    return rmt_receive(
        rx_chan_,
        rx_buf_.data(),
        rx_buf_.size() * sizeof(rmt_symbol_word_t),
        &rcfg
    ) == ESP_OK;
}

void SubGhzService::pushRxSymbols(const rmt_symbol_word_t* src, size_t count) {
    if (src == nullptr || count == 0 || rx_ring_.empty()) return;

    const size_t cap = rx_ring_.size();
    taskENTER_CRITICAL(&rx_ring_mux_);
    for (size_t i = 0; i < count; ++i) {
        const size_t next = (rx_ring_head_ + 1) % cap;
        if (next == rx_ring_tail_) {
            rx_ring_tail_ = (rx_ring_tail_ + 1) % cap;
        }
        rx_ring_[rx_ring_head_] = src[i];
        rx_ring_head_ = next;
    }
    taskEXIT_CRITICAL(&rx_ring_mux_);
}

size_t SubGhzService::popRxSymbols(std::vector<rmt_symbol_word_t>& out, size_t maxSymbols) {
    if (maxSymbols == 0 || rx_ring_.empty()) return 0;

    size_t moved = 0;
    const size_t cap = rx_ring_.size();

    taskENTER_CRITICAL(&rx_ring_mux_);
    while (moved < maxSymbols && rx_ring_tail_ != rx_ring_head_) {
        out.push_back(rx_ring_[rx_ring_tail_]);
        rx_ring_tail_ = (rx_ring_tail_ + 1) % cap;
        ++moved;
    }
    taskEXIT_CRITICAL(&rx_ring_mux_);

    return moved;
}

void SubGhzService::releaseSnifferResources() {
    stopRawSniffer();
}

bool SubGhzService::sendRawFrame(int pin, const std::vector<rmt_symbol_word_t>& items, uint32_t tick_per_us) {
    if (!isConfigured_ || items.empty()) return false;

    // Output
    if (!startTxBitBang()) return false;

    // tick to us
    auto ticks_to_us = [tick_per_us](uint32_t ticks) -> uint32_t {
        if (!tick_per_us) return 0;
        return (ticks + (tick_per_us/2)) / tick_per_us;
    };

    // Bit bang seq
    for (const auto& it : items) {
        gpio_set_level((gpio_num_t)pin, it.level0 ? 1 : 0);
        uint32_t us0 = ticks_to_us(it.duration0);
        if (us0) esp_rom_delay_us(us0);

        gpio_set_level((gpio_num_t)pin, it.level1 ? 1 : 0);
        uint32_t us1 = ticks_to_us(it.duration1);
        if (us1) esp_rom_delay_us(us1);
    }

    // Reset
    gpio_set_level((gpio_num_t)pin, 0);
    return true;
}

bool SubGhzService::startTxBitBang() {
    if (!isConfigured_) return false;

    gpio_config_t io{};
    io.pin_bit_mask = (1ULL << gdo0_);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&io) == ESP_OK;
}

bool SubGhzService::stopTxBitBang() {
    if (!isConfigured_) return false;

    // Force low
    gpio_set_direction((gpio_num_t)gdo0_, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)gdo0_, 0);
    delay(1);

    // Reset to input
    gpio_config_t io{};
    io.pin_bit_mask  = (1ULL << gdo0_);
    io.mode          = GPIO_MODE_INPUT; 
    io.pull_up_en    = GPIO_PULLUP_DISABLE;
    io.pull_down_en  = GPIO_PULLDOWN_ENABLE;
    io.intr_type     = GPIO_INTR_DISABLE;

    ELECHOUSE_cc1101.setSidle();

    return gpio_config(&io) == ESP_OK;
}

bool SubGhzService::sendRawPulse(int pin, int duration) {
    if (!isConfigured_) return false;

    if (duration < 0) {
        gpio_set_level((gpio_num_t)pin, 0);
        delayMicroseconds(-duration);
    } else {
        gpio_set_level((gpio_num_t)pin, 1);
        delayMicroseconds(duration);
    }
    return true;
}

bool SubGhzService::sendRandomBurst(int pin)
{
    if (!isConfigured_) return false;

    constexpr int ITEMS_PER_BURST = 256;   // Number of items
    constexpr int MEAN_US         = 200;   // Mean duration in µs per phase
    constexpr int JITTER_PCT      = 30;    // around the mean

    // Calcul bornes jitter
    int j     = (MEAN_US * JITTER_PCT) / 100;
    int minUs = MEAN_US - j; if (minUs < 1) minUs = 1;
    int maxUs = MEAN_US + j; if (maxUs < minUs) maxUs = minUs;

    auto rnd_between = [&](int lo, int hi) -> uint32_t {
        uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return static_cast<uint32_t>(lo) + (esp_random() % span);
    };

    // Initial random level
    int level = (esp_random() & 1) ? 1 : 0;

    // Bit-bang
    for (int i = 0; i < ITEMS_PER_BURST; ++i) {
        // phase 0
        gpio_set_level((gpio_num_t)pin, level);
        level ^= 1;
        esp_rom_delay_us(rnd_between(minUs, maxUs));

        // phase 1
        gpio_set_level((gpio_num_t)pin, level);
        level ^= 1;
        esp_rom_delay_us(rnd_between(minUs, maxUs));
    }

    // Reset
    gpio_set_level((gpio_num_t)pin, 0);
    return true;
}

// Profiles

bool SubGhzService::applyScanProfile(float dataRateKbps,
                                  float rxBwKhz,
                                  uint8_t modulation,
                                  bool packetMode)
{
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setCCMode(packetMode ? 0 : 1);
    ELECHOUSE_cc1101.setModulation(modulation);
    ELECHOUSE_cc1101.setDRate(dataRateKbps);
    ELECHOUSE_cc1101.setRxBW(rxBwKhz);
    ELECHOUSE_cc1101.setSyncMode(0);
    ELECHOUSE_cc1101.setWhiteData(false);
    ELECHOUSE_cc1101.setCrc(false);
    ELECHOUSE_cc1101.setCRC_AF(false);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setLengthConfig(1);
    ELECHOUSE_cc1101.setPacketLength(0xFF);
    ELECHOUSE_cc1101.SetRx(mhz_);
    // setCCMode() rewrites IOCFG2, which drives the Cap's RF_SW1 line.
    selectRfPathFor(mhz_, true);
    return true;
}

bool SubGhzService::applyDefaultProfile(float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setPktFormat(0);   // PKT_FORMAT=0 (packet mode normal)
    ELECHOUSE_cc1101.setLengthConfig(1);// 1 = variable length
    ELECHOUSE_cc1101.setPacketLength(0xFF);
    ELECHOUSE_cc1101.setCCMode(0);      // RAW / serial out sur GDO0
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2);  // OOK/ASK
    ELECHOUSE_cc1101.setDRate(4.8f);    // kbps
    ELECHOUSE_cc1101.setRxBW(135.0f);   // kHz
    ELECHOUSE_cc1101.setSyncMode(0);
    ELECHOUSE_cc1101.setWhiteData(false);
    ELECHOUSE_cc1101.setCrc(false);
    ELECHOUSE_cc1101.setCRC_AF(false);
    ELECHOUSE_cc1101.setAdrChk(0);
    selectRfPathFor(mhz, true);
    return true;
}

bool SubGhzService::applySniffProfile(float mhz) {
    if (!isConfigured_) return false;
    // CC1101 en "raw/async" OOK
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2);      // 2 = ASK/OOK
    ELECHOUSE_cc1101.setDRate(50);         // 50 kbps
    ELECHOUSE_cc1101.setRxBW(135.0f);     // 58–200
    ELECHOUSE_cc1101.setSyncMode(0);     // no sync
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setDcFilterOff(true);
    ELECHOUSE_cc1101.setPktFormat(3);    // *** ASYNCHRONOUS SERIAL MODE ***
    ELECHOUSE_cc1101.SetRx(mhz);
    selectRfPathFor(mhz);
    return true;
}

bool SubGhzService::applyRawSendProfile(float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2);   // OOK/ASK
    ELECHOUSE_cc1101.setDRate(50);
    ELECHOUSE_cc1101.setRxBW(135.0f);
    ELECHOUSE_cc1101.setSyncMode(0);
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setDcFilterOff(true);
    ELECHOUSE_cc1101.setPktFormat(3);    // ASYNC SERIAL MODE (GDO0 = data)
    ELECHOUSE_cc1101.SetTx();
    selectRfPathFor(mhz);
    return true;
}

bool SubGhzService::applyPresetByName(const std::string& name, float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    selectRfPathFor(mhz);

    // Default
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setCRC_AF(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setSyncMode(0);

    if (name == "FuriHalSubGhzPresetOok270Async") {
        ELECHOUSE_cc1101.setModulation(2); // OOK
        ELECHOUSE_cc1101.setRxBW(270.0f);
        ELECHOUSE_cc1101.setDRate(10.0f);
        ELECHOUSE_cc1101.setPktFormat(3); // async serial
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPresetOok650Async") {
        ELECHOUSE_cc1101.setModulation(2);
        ELECHOUSE_cc1101.setRxBW(650.0f);
        ELECHOUSE_cc1101.setDRate(10.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPreset2FSKDev238Async") {
        ELECHOUSE_cc1101.setModulation(0); // 2-FSK
        ELECHOUSE_cc1101.setDeviation(2.380f);
        ELECHOUSE_cc1101.setRxBW(238.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPreset2FSKDev476Async") {
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setDeviation(47.607f);
        ELECHOUSE_cc1101.setRxBW(476.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPresetMSK99_97KbAsync") {
        ELECHOUSE_cc1101.setModulation(4); // MSK
        ELECHOUSE_cc1101.setDeviation(47.607f);
        ELECHOUSE_cc1101.setDRate(99.97f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPresetGFSK9_99KbAsync") {
        ELECHOUSE_cc1101.setModulation(1); // GFSK
        ELECHOUSE_cc1101.setDeviation(19.043f);
        ELECHOUSE_cc1101.setDRate(9.996f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }

    // RcSwitch:
    char* end=nullptr;
    long proto = std::strtol(name.c_str(), &end, 10);
    if (end != name.c_str()) {
        ELECHOUSE_cc1101.setModulation(2); // OOK
        ELECHOUSE_cc1101.setRxBW(270.0f);
        ELECHOUSE_cc1101.setDRate(10.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }

    // Fallback: OOK async
    return applyRawSendProfile(mhz);
}

bool SubGhzService::sendTimingsOOK_(const std::vector<int32_t>& timings) {
    if (!startTxBitBang()) return false;
    int level = 1; // start HIGH
    for (int32_t us : timings) {
        gpio_set_level((gpio_num_t)gdo0_, level);
        if (us > 0) esp_rom_delay_us((uint32_t)us);
        level ^= 1; // alternate
    }
    gpio_set_level((gpio_num_t)gdo0_, 0);
    stopTxBitBang();
    return true;
}

static inline void appendPair(std::vector<int32_t>& v, int hi_us, int lo_us) {
    if (hi_us>0) v.push_back(hi_us);
    if (lo_us>0) v.push_back(lo_us);
}

bool SubGhzService::sendRcSwitch_(uint64_t key, uint16_t bits, int te_us, int proto, int repeat) {
    if (!bits) return false;
    if (te_us <= 0) te_us = 350;      // défaut
    if (repeat <= 0) repeat = 10;

    // Usual protocols
    int sync_hi=0, sync_lo=0, zero_hi=0, zero_lo=0, one_hi=0, one_lo=0;

    if (proto == 1) {            // 350us, sync 1:31, zero 1:3, one 3:1
        sync_hi = te_us * 1;  sync_lo = te_us * 31;
        zero_hi = te_us * 1;  zero_lo = te_us * 3;
        one_hi  = te_us * 3;  one_lo  = te_us * 1;
    } else if (proto == 2) {     // 650us, sync 1:10, zero 1:2, one 2:1
        sync_hi = te_us * 1;  sync_lo = te_us * 10;
        zero_hi = te_us * 1;  zero_lo = te_us * 2;
        one_hi  = te_us * 2;  one_lo  = te_us * 1;
    } else {                     // “11” commonly used: 350us, sync 1:23, zero 1:2, one 2:1
        proto   = 11;
        sync_hi = te_us * 1;  sync_lo = te_us * 23;
        zero_hi = te_us * 1;  zero_lo = te_us * 2;
        one_hi  = te_us * 2;  one_lo  = te_us * 1;
    }

    std::vector<int32_t> timings;
    timings.reserve((bits * 2 + 4) * repeat);

    for (int r = 0; r < repeat; ++r) {
        // sync
        appendPair(timings, sync_hi, sync_lo);

        // bits MSB->LSB
        for (int i = bits - 1; i >= 0; --i) {
            bool b = (key >> i) & 1ULL;
            if (b) appendPair(timings, one_hi, one_lo);
            else   appendPair(timings, zero_hi, zero_lo);
        }
        // inter-frame gap
    }

    return sendTimingsOOK_(timings);
}

bool SubGhzService::sendPrinceton_(uint64_t key, uint16_t bits, int te_us) {
    // Canon PT2262: TE≈350us, 1=1h3l, 0=3h1l, sync=1h31l
    return sendRcSwitch_(key, bits, te_us>0?te_us:350, /*proto*/1, /*repeat*/10);
}

bool SubGhzService::sendBinRaw_(const std::vector<uint8_t>& bytes,
                                int te_us,
                                int bits /*ignored in ref mode*/,
                                bool /*msb_first unused in ref mode*/,
                                bool /*invert unused in ref mode*/) {
    if (bytes.empty()) return false;
    if (te_us <= 0) te_us = 100;

    // Impl. de référence : idle LOW, envoi depuis la FIN de la chaîne binaire,
    // donc: parcourir les octets à rebours, et dans chaque octet LSB->MSB.
    const int total_bits = int(bytes.size()) * 8;
    // // Si tu veux respecter "Bit:", décommente:
    // int limit_bits = (bits > 0 && bits < total_bits) ? bits : total_bits;
    const int limit_bits = total_bits; // référence = envoie tout

    if (!startTxBitBang()) return false;

    // Idle bas
    gpio_set_level((gpio_num_t)gdo0_, 0);

    int sent = 0;

    // Octets depuis la fin
    for (int bi = int(bytes.size()) - 1; bi >= 0 && sent < limit_bits; --bi) {
        uint8_t b = bytes[bi];

        // Bits LSB -> MSB
        for (int i = 0; i < 8 && sent < limit_bits; ++i, ++sent) {
            bool one = (b >> i) & 0x01; // LSB-first
            gpio_set_level((gpio_num_t)gdo0_, one ? 1 : 0);
            esp_rom_delay_us((uint32_t)te_us);
        }
    }

    // Idle bas en fin de trame
    gpio_set_level((gpio_num_t)gdo0_, 0);

    stopTxBitBang();
    return true;
}

bool SubGhzService::sendRawTimings(const std::vector<int32_t>& timings) {
    return sendTimingsRawSigned_(timings);
}

bool SubGhzService::sendTimingsRawSigned_(const std::vector<int32_t>& timings) {
    if (!startTxBitBang()) return false;

    // Idle LOW
    gpio_set_level((gpio_num_t)gdo0_, 0);

    for (int32_t t : timings) {
        if (t == 0) continue;
        bool high = (t > 0);
        uint32_t us = (uint32_t)(high ? t : -t);
        gpio_set_level((gpio_num_t)gdo0_, high ? 1 : 0);
        if (us) esp_rom_delay_us(us);
    }

    // Return to idle LOW
    gpio_set_level((gpio_num_t)gdo0_, 0);
    stopTxBitBang();
    return true;
}

bool SubGhzService::send(const SubGhzFileCommand& cmd) {
    if (!isConfigured_) return false;

    float mhz = cmd.frequency_hz ? (cmd.frequency_hz / 1e6f) : mhz_;
    tune(mhz);  // update antenna (selectRfPathFor) and RX

    if (!applyPresetByName(cmd.preset, mhz)) {
        // fallback async OOK TX
        if (!applyRawSendProfile(mhz)) return false;
    }

    // IMPORTANT: in async TX mode, GDO0 = data input -> we bit-bang on gdo0_
    switch (cmd.protocol) {
        case SubGhzProtocolEnum::RAW:
            return sendRawTimings(cmd.raw_timings);

        case SubGhzProtocolEnum::BinRAW: {
            const int te = cmd.te_us ? cmd.te_us : 100;
            const int total_bits = int(cmd.bitstream_bytes.size()) * 8;
            return sendBinRaw_(cmd.bitstream_bytes, te, total_bits,
                            /*msb_first*/false, /*invert*/false);
        }

        case SubGhzProtocolEnum::RcSwitch: {
            int proto = 11;
            char* end=nullptr;
            long p = std::strtol(cmd.preset.c_str(), &end, 10);
            if (end != cmd.preset.c_str()) proto = (int)p;
            int te = cmd.te_us ? cmd.te_us : ((proto==2)?650:350);
            return sendRcSwitch_(cmd.key, cmd.bits ? cmd.bits : 24, te, proto, /*repeat*/10);
        }

        case SubGhzProtocolEnum::Princeton:
            return sendPrinceton_(cmd.key, cmd.bits ? cmd.bits : 24, cmd.te_us ? cmd.te_us : 350);

        default: {
            // Fallback CAME/HOLTEK/NICE/unk like RcSwitch(11)
            // TE 270 µs, 10 rep
            if (!cmd.key) return false;
            const int te   = cmd.te_us ? cmd.te_us : 270;
            const int bits = cmd.bits  ? cmd.bits  : 24;
            return sendRcSwitch_(cmd.key, bits, te, /*proto*/11, /*repeat*/10);
        }
    }
}

void SubGhzService::deinitRfModule() {
    releaseSnifferResources();

    if (!isConfigured_) return;

    // Stop the CC1101 - exit RX/TX, turn off frequency synthesizer
    ELECHOUSE_cc1101.setSidle();
    
    // Set GDO0 pin LOW
    pinMode(gdo0_, OUTPUT);
    digitalWrite(gdo0_, LOW);
    
    // Set CS pin HIGH and end SPI transaction
    ELECHOUSE_cc1101.SpiEnd();
    
    // Also set CS as output and high to be safe
    pinMode(ss_, OUTPUT);
    digitalWrite(ss_, HIGH);
    
    // Return to RX mode on last configured frequency
    ELECHOUSE_cc1101.SetRx(mhz_);
    selectRfPathFor(mhz_);
}


// Tembed S3 CC1101 specific

void SubGhzService::initTembed() {
    // POWER
    const int BOARD_PWR_EN   = 15;

    // I2C
    const int BOARD_SDA_PIN  = 18;
    const int BOARD_SCL_PIN  = 8;

    // TFT
    const int BOARD_TFT_CS   = 41;

    // TF card
    const int BOARD_SD_CS    = 13;

    // LoRa / CC1101 front-end
    const int BOARD_LORA_CS  = 12;

    // Disable others spi devices
    pinMode(41, OUTPUT);
    digitalWrite(41, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);

    // Set antenna frequency range, 433 (H/H) as default
    pinMode(rfSw1_, OUTPUT);
    pinMode(rfSw0_, OUTPUT);
    digitalWrite(rfSw1_, HIGH);
    digitalWrite(rfSw0_, HIGH);

    // Power the CC1101
    pinMode(BOARD_PWR_EN, OUTPUT);
    digitalWrite(BOARD_PWR_EN, LOW);
    delay(50);
    digitalWrite(BOARD_PWR_EN, HIGH);
    delay(50);
}

void SubGhzService::selectRfPathFor(float mhz, bool force) {
    // Match the CC1101's supported bands to the available matching networks.
    uint8_t sel; // 0=315, 1=868/915, 2=433
    if (mhz >= 300.0f && mhz <= 348.0f) {
        sel = 0; // 315
    } else if ((mhz >= 387.0f && mhz <= 464.0f)) {
        sel = 2; // 433
    } else if (mhz >= 779.0f && mhz <= 928.0f) {
        sel = 1; // 868/915
    } else {
        // fallback
        sel = 2;
    }

    #ifdef DEVICE_TEMBEDS3CC1101
    if (!force && sel == rfSel_) return;
    switch (sel) {
        case 0: // 315 MHz: SW1=1, SW0=0
            digitalWrite(rfSw1_, HIGH);
            digitalWrite(rfSw0_, LOW);
            break;
        case 1: // 868/915 MHz: SW1=0, SW0=1
            digitalWrite(rfSw1_, LOW);
            digitalWrite(rfSw0_, HIGH);
            break;
        case 2: // 433 MHz: SW1=1, SW0=1
        default:
            digitalWrite(rfSw1_, HIGH);
            digitalWrite(rfSw0_, HIGH);
            break;
    }
    rfSel_ = sel;
    #elif defined(DEVICE_CARDPUTERADV)
    // The official Cap's RF_SW0 is GPIO13; RF_SW1 is CC1101 GDO2.
    // Other CC1101 modules must never have their antenna switch driven here.
    if (!useCardputerAdvCap_) return;
    if (!force && sel == rfSel_) return;

    // Cap CC1101 schematic V0.3 board-level truth table:
    //   315 MHz     -> RF_SW0=0, RF_SW1=1
    //   433 MHz     -> RF_SW0=1, RF_SW1=0
    //   868/915 MHz -> RF_SW0=1, RF_SW1=1
    // Do not use 0/0: the BGS13SN8 defines that state as RF isolation.
    const bool sw0 = (sel != 0); // 433 and 868/915 MHz
    const bool sw1 = (sel != 2); // 315 and 868/915 MHz
    constexpr uint8_t kRfSw0Pin = 13;
    pinMode(kRfSw0Pin, OUTPUT);
    digitalWrite(kRfSw0Pin, sw0 ? HIGH : LOW);

    // IOCFG2=0x2F forces GDO2 LOW; 0x6F uses GDO2_INV to force HIGH.
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, sw1 ? 0x6F : 0x2F);
    rfSel_ = sel;
    #else
    (void)sel;
    (void)force;
    #endif
}
