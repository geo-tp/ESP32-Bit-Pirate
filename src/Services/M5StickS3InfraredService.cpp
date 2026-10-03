#ifdef DEVICE_STICKS3

#include "M5StickS3InfraredService.h"

#include <limits>

#include <M5Unified.h>

#define USE_IRREMOTE_HPP_AS_PLAIN_INCLUDE
#include <IRremote.hpp>

#include "Transformers/SubGhzTransformer.h"

bool M5StickS3InfraredService::armReceiver() {
    rxSymbolCount = rxSymbols.size();
    if (rmtReadAsync(rxPin, rxSymbols.data(), &rxSymbolCount))
        return true;
    log_e("Infrared: cannot arm RMT receiver on GPIO %u", rxPin);
    stopReceiver();
    return false;
}

void M5StickS3InfraredService::configure(uint8_t tx, uint8_t rx) {
    stopReceiver();
    rxPin = rx;
    configureTransmitter(tx);
}

void M5StickS3InfraredService::startReceiver() {
    if (receiving || rxPin == 0xFF)
        return;

    restoreSpeaker = M5.Speaker.isRunning();
    M5.Speaker.end();
    IrSender.IRLedOff();
    receiving = rmtInit(rxPin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_3, 1000000);
    if (!receiving || !rmtSetRxMinThreshold(rxPin, 1) || !rmtSetRxMaxThreshold(rxPin, 20000)) {
        log_e("Infrared: cannot configure RMT receiver on GPIO %u", rxPin);
        stopReceiver();
        return;
    }
    lastFrameEndUs = micros();
    auto *raw = IrReceiver.decodedIRData.rawDataPtr;
    IrReceiver.decodedIRData = {};
    IrReceiver.decodedIRData.rawDataPtr = raw;
    armReceiver();
}

void M5StickS3InfraredService::stopReceiver() {
    if (receiving)
        rmtDeinit(rxPin);
    receiving = false;
    if (restoreSpeaker)
        M5.Speaker.begin();
    restoreSpeaker = false;
}

InfraredCommand M5StickS3InfraredService::receiveInfraredCommand() {

    std::vector<uint16_t> timings;
    uint32_t khz;

    if (!receiveRaw(timings, khz) || timings.size() >= RAW_BUFFER_LENGTH)
        return InfraredCommand{};
    
    // Inject RMT timings into IrReceiver raw params and then call InfraredService::receiveInfraredCommand
    // to trigger command decoding
    auto &irparams = *IrReceiver.decodedIRData.rawDataPtr;
    irparams.rawlen = timings.size() + 1;
    irparams.rawbuf[0] = 0;

    for (size_t i = 0; i < timings.size(); ++i)
    {
        const uint32_t ticks = (timings[i] + MICROS_PER_TICK / 2) / MICROS_PER_TICK;
        if (ticks > std::numeric_limits<IRRawbufType>::max())
            return InfraredCommand{};

        irparams.rawbuf[i + 1] = ticks;
    }

    irparams.OverflowFlag = false;
    irparams.initialGapTicks = initialGapTicks;
    irparams.StateForISR = IR_REC_STATE_STOP;
    IrReceiver.decodedIRData.rawlen = irparams.rawlen;
    IrReceiver.decodedIRData.initialGapTicks = initialGapTicks;
    return InfraredService::receiveInfraredCommand();
}

bool M5StickS3InfraredService::receiveRaw(std::vector<uint16_t> &timings, uint32_t &khz) {
    if (!receiving || !rmtReceiveCompleted(rxPin))
        return false;

    // A full buffer may be truncated; never expose it as a complete frame.
    timings.clear();
    if (rxSymbolCount < rxSymbols.size()) {
        std::vector<rmt_symbol_word_t> symbols;
        symbols.reserve(rxSymbolCount);
        for (size_t i = 0; i < rxSymbolCount; ++i) {
            const auto &raw = rxSymbols[i];
            if (!raw.duration0) break;
            rmt_symbol_word_t symbol{};
            symbol.duration0 = raw.duration0;
            symbol.level0 = raw.level0;
            symbol.duration1 = raw.duration1;
            symbol.level1 = raw.level1;
            symbols.push_back(symbol);
            if (!raw.duration1) break;
        }
        // RX runs at 1 MHz. IR marks are active-low; discard leading idle.
        for (int32_t timing : SubGhzTransformer().symbolsToSignedTimings(symbols, 1)) {
            const bool space = timing > 0;
            const uint32_t us = space ? timing : -timing;
            if (timings.empty() && space) continue;
            if (!timings.empty() && space == ((timings.size() - 1) % 2 != 0)) {
                const uint32_t merged = timings.back() + us;
                if (merged > UINT16_MAX) { timings.clear(); break; }
                timings.back() = merged;
            } else {
                timings.push_back(us);
            }
        }
        if (!timings.empty() && timings.size() % 2 == 0) timings.pop_back();
    }
    const bool valid = !timings.empty();

    if (valid) {
        uint32_t frameUs = 0;
        for (uint16_t us : timings)
            frameUs += us;
        const uint32_t endUs = micros() - 20000; // RMT's trailing idle timeout.
        const uint32_t elapsed = endUs - lastFrameEndUs;
        const uint32_t gapUs = elapsed > frameUs ? elapsed - frameUs : 0;
        initialGapTicks = std::min<uint32_t>(gapUs / MICROS_PER_TICK, UINT16_MAX);
        lastFrameEndUs = endUs;
        khz = InfraredService::IR_DEFAULT_FREQUENCY_KHZ; // Demodulated RX provides envelope timings, not carrier frequency.
    }
    armReceiver();
    return valid;
}

#endif
