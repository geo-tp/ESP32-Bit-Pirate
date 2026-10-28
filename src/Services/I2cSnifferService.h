#pragma once

#include "Interfaces/II2cSnifferService.h"

class I2cSnifferService final : public II2cSnifferService {
public:
    I2cSnifferService() = default;
    ~I2cSnifferService() override { release(); }
    I2cSnifferService(const I2cSnifferService&) = delete;
    I2cSnifferService& operator=(const I2cSnifferService&) = delete;

    bool prepare() override;
    void begin(uint8_t scl, uint8_t sda) override;
    void setAddressFilter(bool enabled, uint8_t address) override;
    bool setup() override;
    const char* backendName() const override;
    const char* lastError() const override;
    void stop() override;
    void release() override;
    bool available() override;
    char read() override;
    void resetBuffer() override;
    bool measureFrequency(uint8_t scl, uint32_t timeoutMs,
                          I2cFrequencyResult& result,
                          const std::function<bool()>& shouldStop) override;

private:
    uint8_t sniffer_scl_pin = 1;
    uint8_t sniffer_sda_pin = 2;
    const char* snifferSetupError = "none";
    uint16_t* eventRing = nullptr;
    uint16_t eventW = 0;
    uint16_t eventR = 0;
    bool eventOverflow = false;
    char* charRing = nullptr;
    uint16_t charW = 0;
    uint16_t charR = 0;
    bool charOverflow = false;
    bool decoderInTransaction = false;
    uint8_t decoderScl = 1;
    uint8_t decoderSda = 1;
    uint8_t decoderBitCount = 0;
    uint8_t decoderByte = 0;
    uint16_t decoderByteCount = 0;
    bool decoderExpectingAck = false;
    uint8_t pendingControlCount = 0;
    uint8_t pendingControlEdges = 0; // bit i: 1 = STOP, 0 = START
    bool addressFilterEnabled = false;
    uint8_t addressFilter = 0;
    bool frameAccepted = true;
    uint32_t lastRmtPumpUs = 0;

    void push_event(uint16_t ev);
    void char_push(char c);
    void text_push_str(const char* s);
    void text_push_hex8(uint8_t value);
    void reset_decoder_state(bool samplePins);
    void decoder_start();
    void decoder_stop();
    void decoder_sample_sda_on_scl_rise();
    void commit_pending_controls();
    void process_scl_edge(bool positive);
    void process_sda_edge(bool positive);
    void process_capture_edge(bool isSda, bool rising);
    void pump_events_to_text();
    bool ensure_buffers_allocated();
    void reset_buffers_and_decoder();
};
