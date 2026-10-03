#pragma once

#include <array>

#include "InfraredService.h"

#include <esp32-hal-rmt.h>

class M5StickS3InfraredService : public InfraredService {
public:
    void configure(uint8_t tx, uint8_t rx) override;
    void startReceiver() override;
    void stopReceiver() override;
    InfraredCommand receiveInfraredCommand() override;
    bool receiveRaw(std::vector<uint16_t> &timings, uint32_t &khz) override;

private:
    uint8_t rxPin = 0xFF;
    bool receiving = false;
    bool restoreSpeaker = false;
    std::array<rmt_data_t, RMT_SYMBOLS_PER_CHANNEL_BLOCK * 3> rxSymbols;
    size_t rxSymbolCount = 0;
    uint32_t lastFrameEndUs = 0;
    uint16_t initialGapTicks = 0;
    bool armReceiver();
};
