#include "I2cSnifferService.h"
#include "Analyzers/I2cFrequencyAnalyzer.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#if SOC_RMT_SUPPORT_DMA
#include "driver/rmt_rx.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include <cstdio>
#include <climits>
#include <cstdlib>
#include <new>
#include <memory>
#endif

//   -DBITPIRATE_I2C_SNIFFER_OPT_O3=0    disable the local -O3 optimization
//   -DBITPIRATE_I2C_SNIFFER_SYMBOL_RING_SIZE=4096  increase per-line backlog
#ifndef BITPIRATE_I2C_SNIFFER_OPT_O3
#define BITPIRATE_I2C_SNIFFER_OPT_O3 1
#endif
#ifndef BITPIRATE_I2C_SNIFFER_SYMBOL_RING_SIZE
#define BITPIRATE_I2C_SNIFFER_SYMBOL_RING_SIZE 2048
#endif
#if BITPIRATE_I2C_SNIFFER_OPT_O3 && defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif

namespace {
#if SOC_RMT_SUPPORT_DMA
// Passive SCL frequency capture.
constexpr uint32_t frequencyResolutionHz = 20000000; // 50 ns
constexpr size_t frequencyDmaSymbols = 512;
constexpr size_t frequencySampleSymbols = 1024;

struct FrequencyCapture {
    rmt_channel_handle_t channel = nullptr;
    rmt_symbol_word_t* dma = nullptr;
    rmt_symbol_word_t* samples = nullptr;
    rmt_receive_config_t receiveConfig = {};
    size_t count = 0;
    esp_err_t receiveError = ESP_OK;
    bool running = false;
    bool enabled = false;

    void stop() {
        __atomic_store_n(&running, false, __ATOMIC_RELEASE);
        // Disable before reading/freeing storage shared with the RX ISR.
        if (enabled) rmt_disable(channel);
        enabled = false;
    }

    ~FrequencyCapture() {
        stop();
        if (channel) rmt_del_channel(channel);
        heap_caps_free(dma);
        heap_caps_free(samples);
    }
};

bool IRAM_ATTR onFrequencyReceived(rmt_channel_handle_t channel,
                                   const rmt_rx_done_event_data_t* event,
                                   void* context) {
    auto& capture = *static_cast<FrequencyCapture*>(context);
    if (!__atomic_load_n(&capture.running, __ATOMIC_ACQUIRE)) return false;

    size_t count = __atomic_load_n(&capture.count, __ATOMIC_RELAXED);
    // Partial RX memory belongs to the driver and may be reused immediately.
    // Copy and publish a bounded sample here; analyze it in task context.
    for (size_t i = 0; i < event->num_symbols && count < frequencySampleSymbols; ++i) {
        capture.samples[count++] = event->received_symbols[i];
    }
    if (event->flags.is_last && count < frequencySampleSymbols) {
        capture.samples[count++].val = 0; // explicit burst boundary
    }
    __atomic_store_n(&capture.count, count, __ATOMIC_RELEASE);

    if (event->flags.is_last && count < frequencySampleSymbols &&
        __atomic_load_n(&capture.running, __ATOMIC_ACQUIRE)) {
        const esp_err_t error = rmt_receive(channel, capture.dma,
            frequencyDmaSymbols * sizeof(rmt_symbol_word_t), &capture.receiveConfig);
        __atomic_store_n(&capture.receiveError, error, __ATOMIC_RELEASE);
    }
    return false;
}
#endif

namespace I2cRmtCapture {
struct Edge {
    uint32_t ticks; // 10 MHz software timestamp, wraps
    bool rising;
};
bool prepare();
bool begin(uint8_t sclGpio, uint8_t sdaGpio);
void stop();
void poll();
bool next(Edge& edge, bool& isSda);
bool takeOverflow();
bool takeAlignmentFailure();
bool drained();
const char* lastError();
void release();
} // namespace I2cRmtCapture

#if SOC_RMT_SUPPORT_DMA
namespace I2cRmtCapture {
static constexpr uint32_t kHz = 10000000; // 100 ns ticks, <=3.276 ms idle
static constexpr uint32_t kIdleNs = 2000000; // 2 ms ends a burst
static constexpr size_t kRxSymbols = 512;
static constexpr size_t kSdaMemSymbols = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;
static constexpr uint16_t kRingSize = 4096; // 16 KiB/line for packed edges
static constexpr uint16_t kRingMask = kRingSize - 1;
static constexpr uint32_t kTickMask = 0x7FFFFFFFUL;
static volatile uint32_t acknowledgedErrors = 0;
static constexpr uint32_t kGuardTicks = 25; // hold 2.5 us after both watermarks
// Retiming and edge extraction in the ISR delayed the next receive.
// ISR copies owned RMT symbols into this single-producer/single-consumer
// queue; pulse conversion, epoch retiming and calibration run in task context.
static constexpr uint16_t kSymbolRingSize = BITPIRATE_I2C_SNIFFER_SYMBOL_RING_SIZE;
static_assert(kSymbolRingSize >= kRxSymbols * 2 &&
              (kSymbolRingSize & (kSymbolRingSize - 1)) == 0 &&
              kSymbolRingSize <= 32768,
              "I2C RMT symbol ring must be a power of two between 1024 and 32768");
static constexpr uint16_t kSymbolRingMask = kSymbolRingSize - 1;
static constexpr uint8_t kChunkSlots = 16;
static constexpr uint8_t kChunkMask = kChunkSlots - 1;
struct Chunk {
    uint16_t start; // ring offset of first copied RMT symbol
    uint16_t count;
    uint32_t callbackUs; // only meaningful for last chunk of a burst
    bool last;
    int8_t tailLevel; // physical level sampled at RX_END, -1 if unused
};

struct Channel {
    rmt_channel_handle_t handle = nullptr;
    gpio_num_t pin = GPIO_NUM_NC;
    rmt_symbol_word_t* symbols = nullptr;
    uint32_t* edges = nullptr; // timestamp in 31 bits, high bit = rising
    volatile uint16_t write = 0;
    volatile uint16_t read = 0;
    volatile uint16_t readyWrite = 0; // published only after end-of-burst epoch calibration
    uint16_t burstStart = 0;
    rmt_symbol_word_t* queuedSymbols = nullptr;
    Chunk chunks[kChunkSlots] = {};
    volatile uint16_t symbolWrite = 0; // ISR producer, task consumer
    volatile uint16_t symbolRead = 0;
    volatile uint8_t chunkWrite = 0;
    volatile uint8_t chunkRead = 0;
    bool discardUntilBoundary = false; // task-only recovery after overflow
    volatile uint32_t progress = 0;
    volatile bool haveProgress = false;
    volatile bool overflow = false;
    volatile bool receiving = false;
    volatile bool inCallback = false;
    volatile bool arming = false;
    volatile bool enabled = false;
    volatile esp_err_t rxError = ESP_OK;
    uint32_t elapsedTicks = 0;
    uint8_t currentLevel = 1;
    bool levelKnown = false;
    bool started = false;
    volatile uint32_t completed = 0;
};
// Allocate the sizable capture state only on first sniff.
// No Channel instances or calibration arrays are retained in .bss at boot.
struct CalEdge { int32_t tick; bool rise; };
struct CaptureMemory {
    Channel clock;
    Channel dataPin;
    CalEdge clocks[256];
    CalEdge data[192];
    char failure[160] = "not initialized";
};
static CaptureMemory* captureMemory = nullptr;
// Indirection avoids reserving large channel/calibration structures at boot.
static inline Channel& scl() { return captureMemory->clock; }
static inline Channel& sda() { return captureMemory->dataPin; }
static volatile bool running = false;
static volatile uint32_t errors = 0;
static volatile uint32_t overflows = 0;
static rmt_receive_config_t rxConfig = {};
static const char* earlyFailure = "not initialized";
static inline char* failureBuffer() { return captureMemory->failure; }
static int32_t sdaSkewTicks = 0;
static uint32_t calibratedSclCompleted = 0;
static uint32_t calibratedSdaCompleted = 0;
static uint32_t calibrationCount = 0;
static volatile uint32_t alignmentFailures = 0;
static uint32_t reportedAlignmentFailures = 0;
// Never feed unrelated, callback-derived clocks into the protocol decoder.
// A successful alignment must be established for every RX burst pair.
static bool alignmentValid = false;

// Called from single-producer per-channel RMT RX ISR, consumed from task.
static inline void emit(Channel& c, uint32_t time, bool rise) {
    const uint16_t w = c.write;
    const uint16_t n = (uint16_t)((w + 1u) & kRingMask);
    if (n == __atomic_load_n(&c.read, __ATOMIC_ACQUIRE)) {
        c.overflow = true;
        ++overflows;
        return;
    }
    // Store RELATIVE ticks. These are not comparable to the other RMT
    // channel until the burst finishes and an absolute epoch is estimated.
    c.edges[w] = (time & 0x7FFFFFFFUL) | (rise ? 0x80000000UL : 0);
    __atomic_store_n(&c.write, n, __ATOMIC_RELEASE);
}
static inline void pulse(Channel& c, uint32_t duration, uint8_t level) {
    // RMT symbols contain two level/duration intervals. The very first
    // level establishes the initial state; every subsequent change is an edge.
    if (duration == 0) return;
    const uint32_t ts = c.elapsedTicks;
    if (!c.levelKnown) {
        // ESP32-S3 RMT begins at the FIRST input transition. This first
        // interval is itself a real edge (typically SDA START or SCL fall),
        // not an initial idle level to discard.
        c.currentLevel = level;
        c.levelKnown = true;
        emit(c, ts, level != 0);
    } else if (level != c.currentLevel) {
        c.currentLevel = level;
        emit(c, ts, level != 0);
    }
    c.elapsedTicks += duration;
}
static bool IRAM_ATTR arm(Channel& c) {
    // The callback and the task-side error retry can race. Allow only one
    // rmt_receive() at a time per channel.
    if (__atomic_exchange_n(&c.arming, true, __ATOMIC_ACQ_REL)) return false;
    // ESP32-S3 RMT starts counting on the FIRST received edge, not when
    // rmt_receive is called. Do not timestamp this burst at arm time.
    c.started = true;
    const esp_err_t e = rmt_receive(c.handle, c.symbols,
                                   kRxSymbols * sizeof(rmt_symbol_word_t), &rxConfig);
    c.rxError = e;
    __atomic_store_n(&c.receiving, e == ESP_OK, __ATOMIC_RELEASE);
    if (e != ESP_OK) ++errors;
    __atomic_store_n(&c.arming, false, __ATOMIC_RELEASE);
    return e == ESP_OK;
}
static bool IRAM_ATTR onDone(rmt_channel_handle_t, const rmt_rx_done_event_data_t* data, void* ctx) {
    auto* c = static_cast<Channel*>(ctx);
    if (!__atomic_load_n(&running, __ATOMIC_ACQUIRE)) return false;
    __atomic_store_n(&c->inCallback, true, __ATOMIC_RELEASE);
    // The driver's partial-RX memory can be overwritten after this callback:
    // copy the complete callback into our owned FIFO BEFORE returning.
    const bool last = data->flags.is_last;
    const uint32_t callbackUs = last ? (uint32_t)esp_timer_get_time() : 0;
    const size_t n = data->num_symbols;
    int8_t tailLevel = -1;
    if (last && n) {
        const auto& tail = data->received_symbols[n - 1u];
        if (tail.duration0 && !tail.duration1) {
            tailLevel = static_cast<int8_t>(gpio_get_level(c->pin));
        }
    }
    const uint16_t w = c->symbolWrite;
    const uint16_t r = __atomic_load_n(&c->symbolRead, __ATOMIC_ACQUIRE);
    const uint8_t cw = c->chunkWrite;
    const uint8_t nextChunk = (uint8_t)((cw + 1u) & kChunkMask);
    if (n > kSymbolRingSize - 1u - ((w - r) & kSymbolRingMask) ||
        n > kRxSymbols ||
        nextChunk == __atomic_load_n(&c->chunkRead, __ATOMIC_ACQUIRE)) {
        c->overflow = true;
        ++overflows;
    } else {
        // No heap allocation, calibration or per-edge atomic ring operations
        // in the interrupt. Copy as 32-bit words to keep the
        // routine IRAM-safe even if the flash cache is momentarily disabled.
        uint16_t dst = w;
        for (size_t i = 0; i < n; ++i) {
            c->queuedSymbols[dst].val = data->received_symbols[i].val;
            dst = (uint16_t)((dst + 1u) & kSymbolRingMask);
        }
        c->chunks[cw] = {w, (uint16_t)n, callbackUs, last, tailLevel};
        __atomic_store_n(&c->symbolWrite, dst, __ATOMIC_RELEASE);
        __atomic_store_n(&c->chunkWrite, nextChunk, __ATOMIC_RELEASE);
    }
    if (data->flags.is_last) {
        c->receiving = false;
        // Rearm as early as possible; the slower decode runs in poll().
        if (__atomic_load_n(&running, __ATOMIC_ACQUIRE)) arm(*c);
    }
    __atomic_store_n(&c->inCallback, false, __ATOMIC_RELEASE);
    return false;
}

// Only the task calls these functions. The ISR never touches edge buffers or
// RMT elapsedTicks, so a new burst can be received while we decode the last.
static void finishBurst(Channel& c, uint32_t callbackUs) {
    const uint32_t endTicks = callbackUs * 10u;
    const uint32_t epoch = endTicks - c.elapsedTicks - kIdleNs / 100u;
    uint16_t i = c.burstStart;
    const uint16_t end = c.write;
    while (i != end) {
        const uint32_t e = c.edges[i];
        c.edges[i] = (((e & kTickMask) + epoch) & kTickMask) | (e & 0x80000000UL);
        i = (uint16_t)((i + 1u) & kRingMask);
    }
    __atomic_store_n(&c.readyWrite, end, __ATOMIC_RELEASE);
    __atomic_store_n(&c.progress, endTicks, __ATOMIC_RELEASE);
    __atomic_store_n(&c.haveProgress, true, __ATOMIC_RELEASE);
    ++c.completed;
    c.burstStart = end;
    c.elapsedTicks = 0;
    c.levelKnown = false;
    c.currentLevel = 1;
}

static void decodeQueued(Channel& c) {
    while (!c.overflow) {
        const uint8_t r = c.chunkRead;
        if (r == __atomic_load_n(&c.chunkWrite, __ATOMIC_ACQUIRE)) break;
        const Chunk chunk = c.chunks[r];
        uint16_t i = chunk.start;
        // After a lost chunk, no timestamps or protocol edges are reliable
        // until the next complete RMT burst begins.
        if (!c.discardUntilBoundary) {
            for (unsigned j = 0; j < chunk.count; ++j) {
                const rmt_symbol_word_t symbol = c.queuedSymbols[i];
                pulse(c, symbol.duration0, symbol.level0);
                pulse(c, symbol.duration1, symbol.level1);
                i = (uint16_t)((i + 1u) & kSymbolRingMask);
            }
            if (chunk.last) {
                // RMT can encode the final physical transition with a zero
                // duration1; corroborate it against the idle GPIO level.
                if (chunk.count) {
                    const uint16_t tailPos = (uint16_t)((chunk.start + chunk.count - 1u) & kSymbolRingMask);
                    const auto tail = c.queuedSymbols[tailPos];
                    if (tail.duration0 && !tail.duration1 && c.levelKnown &&
                        tail.level1 != c.currentLevel &&
                        chunk.tailLevel == static_cast<int>(tail.level1)) {
                        emit(c, c.elapsedTicks, tail.level1 != 0);
                        c.currentLevel = tail.level1;
                    }
                }
                if (!c.overflow) finishBurst(c, chunk.callbackUs);
            }
        } else if (chunk.last) {
            c.discardUntilBoundary = false;
        }
        // Publish free space only after the entire chunk is no longer read.
        __atomic_store_n(&c.symbolRead,
                         (uint16_t)((chunk.start + chunk.count) & kSymbolRingMask),
                         __ATOMIC_RELEASE);
        __atomic_store_n(&c.chunkRead, (uint8_t)((r + 1u) & kChunkMask), __ATOMIC_RELEASE);
    }
}

// Resources survive repeated sniffs; only I2C mode exit frees them.
static void dispose(Channel& c) {
    if (c.handle) {
        if (c.enabled) rmt_disable(c.handle);
        rmt_del_channel(c.handle);
        c.handle = nullptr;
    }
    c.enabled = false;
    if (c.symbols) { heap_caps_free(c.symbols); c.symbols = nullptr; }
    if (c.edges) { heap_caps_free(c.edges); c.edges = nullptr; }
    if (c.queuedSymbols) { heap_caps_free(c.queuedSymbols); c.queuedSymbols = nullptr; }
    c.write = c.read = c.readyWrite = 0;
    c.symbolWrite = c.symbolRead = 0;
    c.chunkWrite = c.chunkRead = 0;
    c.discardUntilBoundary = false;
    c.receiving = false;
    c.inCallback = false;
    c.arming = false;
    c.haveProgress = false;
}

bool prepare() {
    if (captureMemory) return true;
    void* storage = heap_caps_malloc(
        sizeof(CaptureMemory), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    CaptureMemory* mem = storage ? new (storage) CaptureMemory{} : nullptr;
    if (!mem) {
        earlyFailure = "RMT state: ESP_ERR_NO_MEM";
        return false;
    }
    // No hardware channel is installed here: doing so could reconfigure
    // SDA/SCL while the I2C mode itself is using Wire.
    mem->clock.edges = static_cast<uint32_t*>(heap_caps_malloc(
        kRingSize * sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    mem->dataPin.edges = static_cast<uint32_t*>(heap_caps_malloc(
        kRingSize * sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    mem->clock.queuedSymbols = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        kSymbolRingSize * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    mem->dataPin.queuedSymbols = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        kSymbolRingSize * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    mem->clock.symbols = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        kRxSymbols * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA));
    mem->dataPin.symbols = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        kRxSymbols * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!mem->clock.edges || !mem->dataPin.edges ||
        !mem->clock.queuedSymbols || !mem->dataPin.queuedSymbols ||
        !mem->clock.symbols || !mem->dataPin.symbols) {
        heap_caps_free(mem->clock.edges);
        heap_caps_free(mem->dataPin.edges);
        heap_caps_free(mem->clock.queuedSymbols);
        heap_caps_free(mem->dataPin.queuedSymbols);
        heap_caps_free(mem->clock.symbols);
        heap_caps_free(mem->dataPin.symbols);
        mem->~CaptureMemory();
        heap_caps_free(mem);
        earlyFailure = "RMT buffers: ESP_ERR_NO_MEM";
        return false;
    }
    captureMemory = mem;
    return true;
}

static void resetChannel(Channel& c) {
    c.write = c.read = c.readyWrite = 0;
    c.symbolWrite = c.symbolRead = 0;
    c.chunkWrite = c.chunkRead = 0;
    c.discardUntilBoundary = false;
    c.burstStart = 0;
    c.elapsedTicks = 0;
    c.progress = 0;
    c.haveProgress = false;
    c.overflow = false;
    c.rxError = ESP_OK;
    c.completed = 0;
    c.currentLevel = 1;
    c.levelKnown = false;
    c.started = false;
    c.receiving = false;
    c.inCallback = false;
    c.arming = false;
}

static bool init(Channel& c, uint8_t pin, bool dma, const char* label) {
    // A GPIO change while staying in I2C mode requires a new channel route.
    // The expensive capture buffers remain allocated across this change.
    if (c.handle && c.pin != static_cast<gpio_num_t>(pin)) {
        if (c.enabled) rmt_disable(c.handle);
        rmt_del_channel(c.handle);
        c.handle = nullptr;
        c.enabled = false;
    }
    resetChannel(c);
    c.pin = static_cast<gpio_num_t>(pin);
    if (!c.handle) {
        rmt_rx_channel_config_t cfg = {};
        cfg.gpio_num = static_cast<gpio_num_t>(pin);
        cfg.clk_src = RMT_CLK_SRC_DEFAULT;
        cfg.resolution_hz = kHz;
        cfg.mem_block_symbols = dma ? kRxSymbols : kSdaMemSymbols;
        cfg.flags.with_dma = dma;
        esp_err_t e = rmt_new_rx_channel(&cfg, &c.handle);
        if (e != ESP_OK) {
            snprintf(failureBuffer(), sizeof(captureMemory->failure), "%s new channel: %s", label, esp_err_to_name(e));
            return false;
        }
        rmt_rx_event_callbacks_t cb = {};
        cb.on_recv_done = onDone;
        e = rmt_rx_register_event_callbacks(c.handle, &cb, &c);
        if (e != ESP_OK) {
            snprintf(failureBuffer(), sizeof(captureMemory->failure), "%s register callbacks: %s", label, esp_err_to_name(e));
            rmt_del_channel(c.handle);
            c.handle = nullptr;
            return false;
        }
    }
    const esp_err_t e = rmt_enable(c.handle);
    if (e != ESP_OK) {
        snprintf(failureBuffer(), sizeof(captureMemory->failure), "%s RMT enable: %s", label, esp_err_to_name(e));
        return false;
    }
    c.enabled = true;
    return true;
}

void stop() {
    running = false;
    if (!captureMemory) return;
    Channel* channels[2] = {&scl(), &sda()};
    for (Channel* c : channels) {
        if (c->handle && c->enabled) {
            rmt_disable(c->handle);
            c->enabled = false;
        }
        resetChannel(*c);
    }
    alignmentValid = false;
}

void release() {
    stop();
    if (!captureMemory) return;
    dispose(scl());
    dispose(sda());
    CaptureMemory* old = captureMemory;
    captureMemory = nullptr;
    old->~CaptureMemory();
    heap_caps_free(old);
}

bool begin(uint8_t sclPin, uint8_t sdaPin) {
    stop();
    if (!prepare()) return false;
    errors = overflows = acknowledgedErrors = 0;
    sdaSkewTicks = 0;
    calibratedSclCompleted = calibratedSdaCompleted = calibrationCount = 0;
    alignmentFailures = reportedAlignmentFailures = 0;
    alignmentValid = false;
    snprintf(failureBuffer(), sizeof(captureMemory->failure), "none");
    rxConfig = {};
    rxConfig.signal_range_min_ns = 100;  // one 10-MHz tick
    rxConfig.signal_range_max_ns = kIdleNs;
    rxConfig.flags.en_partial_rx = 1;
    if (!init(scl(), sclPin, true, "SCL DMA") ||
        !init(sda(), sdaPin, false, "SDA non-DMA")) {
        stop();
        return false;
    }
    running = true;
    if (!arm(scl()) || !arm(sda())) {
        snprintf(failureBuffer(), sizeof(captureMemory->failure), "RMT receive start failed (SCL=%d SDA=%d)", (int)scl().rxError, (int)sda().rxError);
        stop();
        return false;
    }
    return true;
}
void poll() {
    if (!running || !captureMemory) return;
    if (!scl().overflow && !sda().overflow) {
        decodeQueued(scl());
        decodeQueued(sda());
    }
    // Retry any ISR rearm failure from task context.
    if (!__atomic_load_n(&scl().receiving, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&scl().inCallback, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&scl().arming, __ATOMIC_ACQUIRE)) arm(scl());
    if (!__atomic_load_n(&sda().receiving, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&sda().inCallback, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&sda().arming, __ATOMIC_ACQUIRE)) arm(sda());
}
static inline bool before(uint32_t a, uint32_t b) {
    return (((a - b) & kTickMask) & 0x40000000UL) != 0;
}
static inline bool peek(Channel& c, uint32_t& e) {
    const uint16_t r = c.read;
    if (r == __atomic_load_n(&c.readyWrite, __ATOMIC_ACQUIRE)) return false;
    e = c.edges[r];
    return true;
}
// RMT RX starts at the FIRST GPIO edge on *each* channel. An RX_DONE ISR
// timestamp is not a hardware epoch; the two ISRs can arrive dozens of us
// apart. The previous ±10us callback-based calibration consequently stayed
// at cal=0 in real captures, while next() decoded their misordered edges.
//
// Instead, align the beginning of complete RX bursts to I2C itself: on an
// idle bus SDA falls for START before SCL's first falling clock edge. Both
// RMT streams then measure intervals in the same 10MHz clock domain. Search
// the physically allowed START-hold interval and score the *entire* waveform
// for SCL-high control transitions and valid 9-clock I2C byte boundaries.
// This is a protocol-based estimate, NOT a shared hardware timestamp. If it
// cannot be established, discard the ambiguous burst rather than print fake
// START/STOP/ACK. Alignments are not guaranteed for every waveform.
static int32_t relativeTime(uint32_t tick, uint32_t origin) {
    const uint32_t d = (tick - origin) & kTickMask;
    return (d & 0x40000000UL) ? (int32_t)(d - 0x80000000UL) : (int32_t)d;
}
static size_t sample(Channel& c, CalEdge* out, size_t maxCount, uint32_t origin) {
    uint16_t idx = __atomic_load_n(&c.read, __ATOMIC_ACQUIRE);
    const uint16_t end = __atomic_load_n(&c.readyWrite, __ATOMIC_ACQUIRE);
    size_t n = 0;
    while (idx != end && n < maxCount) {
        const uint32_t e = c.edges[idx];
        out[n++] = { relativeTime(e & kTickMask, origin), (e & 0x80000000UL) != 0 };
        idx = (uint16_t)((idx + 1u) & kRingMask);
    }
    return n;
}
static int evaluateSkew(const CalEdge* clk, size_t nc,
                        const CalEdge* dat, size_t nd, int shift) {
    size_t i = 0, j = 0;
    bool high = true;
    bool active = false;
    int rises = 0;
    int score = 0;
    int goodFrames = 0;
    const int32_t lastClockInterval = (nc > 1) ? clk[nc - 1].tick - clk[nc - 2].tick : 100;
    const int32_t tailWindow = (lastClockInterval > 0) ? ((lastClockInterval * 3 < 3000) ? lastClockInterval * 3 : 3000) : 150;
    // Retain correct signal states and score only actual I2C control edges.
    while (i < nc || j < nd) {
        // Calibration samples are bounded independently (256 SCL, 192
        // SDA). Never score SDA from later transactions after SCL's sample
        // ends: that would create dozens of spurious START/STOP penalties.
        if (i == nc && (j == nd || dat[j].tick + shift > clk[nc - 1].tick + tailWindow)) break;
        const bool chooseData = j < nd && (i == nc ||
            dat[j].tick + shift < clk[i].tick ||
            (dat[j].tick + shift == clk[i].tick && clk[i].rise));
        if (!chooseData) {
            high = clk[i].rise;
            if (high && active) ++rises;
            ++i;
        } else {
            if (high) {
                if (!dat[j].rise) {
                    if (active) {
                        if (rises >= 9 && (rises % 9) <= 1) {
                            score += 120;
                            ++goodFrames; // valid repeated START boundary
                        } else score -= 100;
                    }
                    active = true;
                    rises = 0;
                    score += 10;
                } else {
                    if (active && rises >= 9 && (rises % 9) <= 1) {
                        score += 110;
                        ++goodFrames;
                    } else score -= 100;
                    active = false;
                }
            } else {
                ++score;
            }
            ++j;
        }
    }
    // A short accidental alignment can create one plausible control edge.
    // Require at least one complete byte-aligned frame before decoding.
    return goodFrames ? score : INT_MIN / 4;
}
static void calibrateIfReady() {
    const uint32_t sc = __atomic_load_n(&scl().completed, __ATOMIC_ACQUIRE);
    const uint32_t sd = __atomic_load_n(&sda().completed, __ATOMIC_ACQUIRE);
    if (sc == calibratedSclCompleted || sd == calibratedSdaCompleted) return;

    // Stay within the normal GPIO rings; no dynamic allocation in ISR.
    CalEdge* const clocks = captureMemory->clocks;
    CalEdge* const data = captureMemory->data;
    uint32_t first;
    if (!peek(scl(), first)) return;
    const uint32_t origin = first & kTickMask;
    const size_t nc = sample(scl(), clocks, 256, origin);
    const size_t nd = sample(sda(), data, 192, origin);
    if (nc < 18 || nd < 3) return;

    // Anchor SDA's START falling edge to a FALLING SCL edge after it.
    // Do not limit the absolute ISR-offset correction to ±10us: the
    // callback delay can be much larger, even on a 100kHz bus.
    int bestScore = INT_MIN;
    int bestShift = 0;
    // At identical scores, prefer SDA slightly earlier within the verified
    // setup/hold window. The old smallest-hold tie break moved SDA later,
    // which can classify the preparatory SDA release before a repeated START
    // as a STOP at Fast-mode rates while leaving every address byte correct.
    int bestHold = INT_MIN;
    for (size_t ci = 0, triedClocks = 0; ci < nc && triedClocks < 1; ++ci) {
        if (clocks[ci].rise) continue;
        ++triedClocks;
        int32_t lowTicks = 0;
        for (size_t next = ci + 1; next < nc; ++next) {
            if (clocks[next].rise) {
                lowTicks = clocks[next].tick - clocks[ci].tick;
                break;
            }
        }
        if (lowTicks <= 0) continue;
        // At 100kHz tHD;STA can be ~4-5us; at 400kHz ~0.6us.
        // Include the wider legal/implementation-dependent hold interval.
        const int maxHold = (int)((lowTicks + 3 < 3000) ? lowTicks + 3 : 3000);
        for (size_t di = 0, triedData = 0; di < nd && triedData < 1; ++di) {
            if (data[di].rise) continue;
            ++triedData;
            for (int hold = 1; hold <= maxHold; hold += 3) {
                const int shift = clocks[ci].tick - data[di].tick - hold;
                const int score = evaluateSkew(clocks, nc, data, nd, shift);
                if (score > bestScore || (score == bestScore &&
                    hold > bestHold)) {
                    bestScore = score;
                    bestShift = shift;
                    bestHold = hold;
                }
            }
        }
    }
    calibratedSclCompleted = sc;
    calibratedSdaCompleted = sd;
    // A score >=90 entails at least one complete nine-clock frame and a
    // reasonable number of SDA changes. Never trust a failed calibration.
    if (bestScore >= 90) {
        sdaSkewTicks = bestShift;
        alignmentValid = true;
        ++calibrationCount;
    } else {
        alignmentValid = false;
        ++alignmentFailures;
        // Drop both ambiguous ready queues. RMT has no common epoch, so
        // keeping them would render all following bytes meaningless.
        __atomic_store_n(&scl().read, __atomic_load_n(&scl().readyWrite, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
        __atomic_store_n(&sda().read, __atomic_load_n(&sda().readyWrite, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    }
}
bool next(Edge& result, bool& isSda) {
    if (!running || !captureMemory || !scl().haveProgress || !sda().haveProgress) return false;
    calibrateIfReady();
    if (!alignmentValid ||
        __atomic_load_n(&scl().completed, __ATOMIC_ACQUIRE) != calibratedSclCompleted ||
        __atomic_load_n(&sda().completed, __ATOMIC_ACQUIRE) != calibratedSdaCompleted) return false;
    const uint32_t limitScl = __atomic_load_n(&scl().progress, __ATOMIC_ACQUIRE);
    const uint32_t limitSda = __atomic_load_n(&sda().progress, __ATOMIC_ACQUIRE);
    const uint32_t limit = before(limitScl, limitSda) ? limitScl : limitSda;
    uint32_t a = 0, b = 0;
    const bool ha = peek(scl(), a), hb = peek(sda(), b);
    if (!ha && !hb) return false;
    const uint32_t ta = a & 0x7FFFFFFFUL;
    const uint32_t tb = ((b & kTickMask) + sdaSkewTicks) & kTickMask;
    bool chooseSda = !ha || (hb && (ta == tb ? ((a & 0x80000000UL) != 0) : before(tb, ta)));
    // Compare wrapped 31-bit time values. No two enqueued events should be
    // separated by 2^30 ticks (107 seconds at 10MHz).
    const uint32_t t = chooseSda ? tb : ta;
    if (before(limit - kGuardTicks, t)) return false;
    Channel& c = chooseSda ? sda() : scl();
    __atomic_store_n(&c.read, (uint16_t)((c.read + 1u) & kRingMask), __ATOMIC_RELEASE);
    result.ticks = t;
    result.rising = ((chooseSda ? b : a) & 0x80000000UL) != 0;
    isSda = chooseSda;
    return true;
}
bool drained() {
    // The decoder may finalize a tail STOP only after both RMT raw queues
    // and both finalized edge queues have been fully consumed.
    return running && captureMemory &&
           scl().read == __atomic_load_n(&scl().readyWrite, __ATOMIC_ACQUIRE) &&
           sda().read == __atomic_load_n(&sda().readyWrite, __ATOMIC_ACQUIRE) &&
           scl().chunkRead == __atomic_load_n(&scl().chunkWrite, __ATOMIC_ACQUIRE) &&
           sda().chunkRead == __atomic_load_n(&sda().chunkWrite, __ATOMIC_ACQUIRE) &&
           !scl().discardUntilBoundary && !sda().discardUntilBoundary;
}
bool takeAlignmentFailure() {
    if (!captureMemory) return false;
    const uint32_t count = __atomic_load_n(&alignmentFailures, __ATOMIC_ACQUIRE);
    if (count == reportedAlignmentFailures) return false;
    reportedAlignmentFailures = count;
    return true;
}
bool takeOverflow() {
    if (!captureMemory) return false;
    const uint32_t nowErrors = errors;
    const bool bad = scl().overflow || sda().overflow || nowErrors != acknowledgedErrors;
    acknowledgedErrors = nowErrors;
    if (bad) {
        // Drop both queues and discard the remainder of any in-flight burst.
        // Published bytes may already have reached the output; future data
        // must begin from fresh, complete RX burst pairs.
        Channel* const channels[2] = {&scl(), &sda()};
        for (Channel* c : channels) {
            __atomic_store_n(&c->read, __atomic_load_n(&c->write, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
            __atomic_store_n(&c->readyWrite, c->read, __ATOMIC_RELEASE);
            __atomic_store_n(&c->chunkRead, __atomic_load_n(&c->chunkWrite, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
            __atomic_store_n(&c->symbolRead, __atomic_load_n(&c->symbolWrite, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
            c->burstStart = c->write;
            c->elapsedTicks = 0;
            c->levelKnown = false;
            c->discardUntilBoundary = true;
            c->overflow = false;
        }
        alignmentValid = false;
        calibratedSclCompleted = scl().completed;
        calibratedSdaCompleted = sda().completed;
    }
    return bad;
}
const char* lastError() { return captureMemory ? failureBuffer() : earlyFailure; }
}
#else
namespace I2cRmtCapture {
bool prepare() { return false; }
bool begin(uint8_t, uint8_t) { return false; }
void stop() {}
void poll() {}
bool next(Edge&, bool&) { return false; }
bool takeOverflow() { return false; }
bool takeAlignmentFailure() { return false; }
bool drained() { return false; }
const char* lastError() { return "RMT DMA unsupported"; }
void release() {}
}
#endif

} // anonymous namespace

// ---------------------------------------------------------------------------
// Decoded event ring (main-context producer -> text formatter consumer)
// ---------------------------------------------------------------------------

#define TAG_START    0x1
#define TAG_STOP     0x2
#define TAG_DATA     0x3
#define TAG_ADDR     0x4
#define TAG_ACK      0x5
#define TAG_OVERFLOW 0x6
#define TAG_UNSYNC 0x7

static inline uint16_t PACK_EVENT(uint16_t tag, uint16_t value) {
    return (uint16_t)((tag << 12) | (value & 0x0FFF));
}
static inline uint8_t EVENT_TAG(uint16_t ev) {
    return (uint8_t)((ev >> 12) & 0x0F);
}
static inline uint16_t EVENT_VAL(uint16_t ev) {
    return (uint16_t)(ev & 0x0FFF);
}

#define EVENT_RING_ORDER 11
#define EVENT_RING_SIZE  (1u << EVENT_RING_ORDER)
#define EVENT_RING_MASK  (EVENT_RING_SIZE - 1u)





void I2cSnifferService::push_event(uint16_t ev) {
    if (eventRing == nullptr) {
        return;
    }

    const uint16_t next = (uint16_t)((eventW + 1u) & EVENT_RING_MASK);
    if (next == eventR) {
        eventOverflow = true;
        return;
    }

    eventRing[eventW] = ev;
    eventW = next;
}

// ---------------------------------------------------------------------------
// Output character ring
// ---------------------------------------------------------------------------

#define CHAR_RING_ORDER 13
#define CHAR_RING_SIZE  (1u << CHAR_RING_ORDER)
#define CHAR_RING_MASK  (CHAR_RING_SIZE - 1u)





void I2cSnifferService::char_push(char c) {
    if (charRing == nullptr) {
        return;
    }

    const uint16_t next = (uint16_t)((charW + 1u) & CHAR_RING_MASK);
    if (next == charR) {
        // Do not move charR here: overwriting the oldest bytes produces the
        // misleading truncated lines seen with the previous implementation.
        charOverflow = true;
        return;
    }

    charRing[charW] = c;
    charW = next;
}

void I2cSnifferService::text_push_str(const char* s) {
    while (*s) {
        char_push(*s++);
    }
}

void I2cSnifferService::text_push_hex8(uint8_t value) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    char_push('0');
    char_push('x');
    char_push(HEX_DIGITS[(value >> 4) & 0x0F]);
    char_push(HEX_DIGITS[value & 0x0F]);
}

// ---------------------------------------------------------------------------
// I2C decoder state (main context only)
// ---------------------------------------------------------------------------








// SDA transitions observed while the *decoded* SCL level is HIGH are not
// immediately trusted. A missing SCL falling capture can otherwise turn SDA
// data transitions into phantom START/STOP pairs.


static constexpr uint8_t MAX_PENDING_CONTROLS = 8;






void I2cSnifferService::reset_decoder_state(bool samplePins) {
    decoderInTransaction = false;
    pendingControlCount = 0;
    pendingControlEdges = 0;
    decoderBitCount = 0;
    decoderByte = 0;
    decoderByteCount = 0;
    decoderExpectingAck = false;
    frameAccepted = !addressFilterEnabled;

    if (samplePins) {
        decoderScl = (uint8_t)(gpio_get_level((gpio_num_t)sniffer_scl_pin) != 0);
        decoderSda = (uint8_t)(gpio_get_level((gpio_num_t)sniffer_sda_pin) != 0);
    } else {
        decoderScl = 1;
        decoderSda = 1;
    }
}

void I2cSnifferService::decoder_start() {
    // START and repeated START both begin a fresh address phase.
    if (!addressFilterEnabled) {
        push_event(PACK_EVENT(TAG_START, 0));
    }

    decoderInTransaction = true;
    decoderBitCount = 0;
    decoderByte = 0;
    decoderByteCount = 0;
    decoderExpectingAck = false;
    frameAccepted = !addressFilterEnabled;
}

void I2cSnifferService::decoder_stop() {
    if (!decoderInTransaction) {
        return;
    }

    if (frameAccepted) {
        push_event(PACK_EVENT(TAG_STOP, 0));
    }

    decoderInTransaction = false;
    decoderBitCount = 0;
    decoderByte = 0;
    decoderByteCount = 0;
    decoderExpectingAck = false;
    frameAccepted = !addressFilterEnabled;
}

void I2cSnifferService::decoder_sample_sda_on_scl_rise() {
    if (!decoderInTransaction) {
        return;
    }

    if (decoderExpectingAck) {
        if (frameAccepted) {
            push_event(PACK_EVENT(TAG_ACK, decoderSda ? 1u : 0u));
        }
        decoderExpectingAck = false;
        decoderBitCount = 0;
        decoderByte = 0;
        return;
    }

    decoderByte = (uint8_t)((decoderByte << 1) | (decoderSda & 0x01u));
    decoderBitCount++;

    if (decoderBitCount != 8) {
        return;
    }

    if (decoderByteCount == 0) {
        const uint8_t addr = (uint8_t)((decoderByte >> 1) & 0x7F);
        frameAccepted = !addressFilterEnabled || (addr == addressFilter);

        if (frameAccepted) {
            // With a filter enabled, defer START until the address byte proves
            // this phase belongs to the requested device.
            if (addressFilterEnabled) {
                push_event(PACK_EVENT(TAG_START, 0));
            }
            push_event(PACK_EVENT(TAG_ADDR, decoderByte));
        }
    } else if (frameAccepted) {
        push_event(PACK_EVENT(TAG_DATA, decoderByte));
    }

    decoderByteCount++;
    decoderExpectingAck = true;
}

void I2cSnifferService::commit_pending_controls() {
    for (uint8_t i = 0; i < pendingControlCount; ++i) {
        if (pendingControlEdges & (1u << i)) {
            decoder_stop();
        } else {
            decoder_start();
        }
    }
    pendingControlCount = 0;
    pendingControlEdges = 0;
}

void I2cSnifferService::process_scl_edge(bool positive) {

    if (positive) {
        if (decoderScl != 0) {
            // The falling edge was not captured. Normally SDA transitions
            // since the preceding rise belong to this missing LOW phase and
            // must not create phantom START/STOP events.
            //
            // Exception: when idle, a single falling SDA edge can be the
            // real START whose *first* SCL falling edge was missed. Accept
            // that START before sampling this rising edge; otherwise the
            // first address bit is lost (e.g. 0x39 W -> 0x72 R).
            const bool idleStart = !decoderInTransaction &&
                                   pendingControlCount == 1 &&
                                   pendingControlEdges == 0;
            if (idleStart) {
                commit_pending_controls();
            } else {
                pendingControlCount = 0;
                pendingControlEdges = 0;
            }
        }
        decoderScl = 1;
        decoder_sample_sda_on_scl_rise();
    } else {
        // A genuine HIGH -> LOW clock transition confirms SDA control edges
        // observed while HIGH, including a quick STOP -> START scan boundary.
        if (decoderScl != 0) {
            commit_pending_controls();
        } else {
            // LOW -> LOW implies a missed rise. Byte alignment is unknown:
            // don't invent bits from the incomplete clock cycle.
            decoderInTransaction = false;
            decoderBitCount = 0;
            decoderExpectingAck = false;
            pendingControlCount = 0;
            pendingControlEdges = 0;
        }
        decoderScl = 0;
    }
}

void I2cSnifferService::process_sda_edge(bool positive) {
    decoderSda = positive ? 1 : 0;
    if (decoderScl == 0) {
        return;
    }

    // Defer classification until the next SCL edge. This also retains the
    // order of a fast STOP -> START pair preceding the next SCL falling edge.
    if (pendingControlCount < MAX_PENDING_CONTROLS) {
        if (positive) {
            pendingControlEdges |= (uint8_t)(1u << pendingControlCount);
        }
        ++pendingControlCount;
    } else {
        // No legitimate scan needs >8 SDA control transitions without SCL.
        // Drop this ambiguous burst rather than emit phantom frames.
        pendingControlCount = 0;
        pendingControlEdges = 0;
        decoderInTransaction = false;
    }
}

void I2cSnifferService::process_capture_edge(bool isSda, bool rising) {
    if (isSda) {
        process_sda_edge(rising);
    } else {
        process_scl_edge(rising);
    }
}

// ---------------------------------------------------------------------------
// Event -> text formatting (main context)
// ---------------------------------------------------------------------------

void I2cSnifferService::pump_events_to_text() {
    static constexpr uint16_t MAX_EVENTS_PER_PUMP = 256;
    uint16_t processed = 0;

    while (processed < MAX_EVENTS_PER_PUMP && eventR != eventW) {
        const uint16_t ev = eventRing[eventR];
        eventR = (uint16_t)((eventR + 1u) & EVENT_RING_MASK);

        switch (EVENT_TAG(ev)) {
            case TAG_START:
                text_push_str("[S] ");
                break;

            case TAG_STOP:
                text_push_str("[P]");
                char_push('\n');
                break;

            case TAG_ADDR: {
                const uint8_t b = (uint8_t)EVENT_VAL(ev);
                text_push_str("ADDR ");
                text_push_hex8((uint8_t)((b >> 1) & 0x7F));
                char_push(' ');
                char_push((b & 0x01u) ? 'R' : 'W');
                char_push(' ');
                break;
            }

            case TAG_DATA:
                text_push_hex8((uint8_t)EVENT_VAL(ev));
                char_push(' ');
                break;

            case TAG_ACK:
                text_push_str(EVENT_VAL(ev) == 0 ? "<ACK> " : "<NACK> ");
                break;

            case TAG_OVERFLOW:
                text_push_str("[!] CAPTURE OVERFLOW - resync at next START");
                char_push('\n');
                break;
            case TAG_UNSYNC:
                text_push_str("[!] RMT UNSYNCHRONIZED - discarded burst");
                char_push('\n');
                break;

            default:
                break;
        }

        processed++;
    }

    if (eventOverflow) {
        eventOverflow = false;
        text_push_str("[!] DECODE BUFFER OVERFLOW");
        char_push('\n');
    }
}

// ---------------------------------------------------------------------------
// RMT capture lifecycle
// ---------------------------------------------------------------------------
bool I2cSnifferService::prepare() {
    if (!I2cRmtCapture::prepare()) {
        snifferSetupError = I2cRmtCapture::lastError();
        return false;
    }
    if (!ensure_buffers_allocated()) {
        snifferSetupError = "failed to allocate sniffer buffers";
        return false;
    }
    return true;
}

bool I2cSnifferService::ensure_buffers_allocated() {
    if (!eventRing) {
        eventRing = static_cast<uint16_t*>(heap_caps_malloc(
            EVENT_RING_SIZE * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!charRing) {
        charRing = static_cast<char*>(heap_caps_malloc(
            CHAR_RING_SIZE * sizeof(char), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    return eventRing && charRing;
}

void I2cSnifferService::reset_buffers_and_decoder() {
    eventW = eventR = 0;
    charW = charR = 0;
    eventOverflow = false;
    charOverflow = false;
    reset_decoder_state(true);
}

void I2cSnifferService::begin(uint8_t scl, uint8_t sda) {
    sniffer_scl_pin = scl;
    sniffer_sda_pin = sda;
}

void I2cSnifferService::setAddressFilter(bool enabled, uint8_t address) {
    addressFilterEnabled = enabled;
    addressFilter = (uint8_t)(address & 0x7F);
    frameAccepted = !enabled;
}

const char* I2cSnifferService::backendName() const { return "RMT RX"; }

const char* I2cSnifferService::lastError() const { return snifferSetupError; }

bool I2cSnifferService::setup() {
    snifferSetupError = "none";
    if (!prepare()) return false;
    // Passive monitor: the live I2C bus must provide its own pull-ups.
    pinMode(sniffer_scl_pin, INPUT);
    pinMode(sniffer_sda_pin, INPUT);
    reset_buffers_and_decoder();
    lastRmtPumpUs = 0;
    if (!I2cRmtCapture::begin(sniffer_scl_pin, sniffer_sda_pin)) {
        snifferSetupError = I2cRmtCapture::lastError();
        I2cRmtCapture::stop();
        return false;
    }
    return true;
}

void I2cSnifferService::stop() {
    I2cRmtCapture::stop();
    reset_decoder_state(true);
}

void I2cSnifferService::release() {
    stop();
    I2cRmtCapture::release();
    eventW = eventR = 0;
    charW = charR = 0;
    uint16_t* oldEvents = eventRing;
    char* oldChars = charRing;
    eventRing = nullptr;
    charRing = nullptr;
    heap_caps_free(oldEvents);
    heap_caps_free(oldChars);
}

bool I2cSnifferService::available() {
    if (!charRing || !eventRing) return false;

    // Serve existing text without expensive re-calibration on every byte.
    // Keep processing acquisition at least every 200us under heavy output.
    const uint32_t nowUs = micros();
    if (charR != charW && (uint32_t)(nowUs - lastRmtPumpUs) < 200u) {
        return true;
    }
    lastRmtPumpUs = nowUs;

    I2cRmtCapture::poll();
    if (I2cRmtCapture::takeOverflow()) {
        reset_decoder_state(true);
        push_event(PACK_EVENT(TAG_OVERFLOW, 0));
    }
    I2cRmtCapture::Edge edge;
    bool isSda = false;
    static constexpr unsigned MAX_RMT_EDGES_PER_PUMP = 2048;
    unsigned processed = 0;
    while (processed < MAX_RMT_EDGES_PER_PUMP &&
           I2cRmtCapture::next(edge, isSda)) {
        process_capture_edge(isSda, edge.rising);
        ++processed;
    }
    if (I2cRmtCapture::takeAlignmentFailure()) {
        reset_decoder_state(true);
        push_event(PACK_EVENT(TAG_UNSYNC, 0));
    }

    // Commit the actual SDA STOP captured by RMT once both RX streams are
    // drained and the bus is HIGH/HIGH. Never invent a STOP from GPIO alone.
    const bool trailingStop = pendingControlCount != 0 &&
        (pendingControlEdges & (1u << (pendingControlCount - 1u))) != 0;
    if (trailingStop && I2cRmtCapture::drained() &&
        gpio_get_level((gpio_num_t)sniffer_scl_pin) != 0 &&
        gpio_get_level((gpio_num_t)sniffer_sda_pin) != 0) {
        commit_pending_controls();
    }

    if (charR == charW) pump_events_to_text();
    if (charOverflow && charR == charW) {
        charOverflow = false;
        text_push_str("[!] OUTPUT BUFFER OVERFLOW");
        char_push('\n');
    }
    return charR != charW;
}

char I2cSnifferService::read() {
    if (!charRing || charR == charW) {
        if (!available()) return '\0';
    }
    const char out = charRing[charR];
    charR = (uint16_t)((charR + 1u) & CHAR_RING_MASK);
    return out;
}

void I2cSnifferService::resetBuffer() {
    if (!charRing || !eventRing) return;
    reset_buffers_and_decoder();
}

bool I2cSnifferService::measureFrequency(uint8_t scl, uint32_t timeoutMs,
                                        I2cFrequencyResult& result,
                                        const std::function<bool()>& shouldStop) {
    result = {};
    // A previous sniff retains its RMT channels. Release them before claiming
    // the single DMA RX channel, and leave frequency capture entirely lazy.
    release();
    snifferSetupError = "none";
#if SOC_RMT_SUPPORT_DMA
    if (!GPIO_IS_VALID_GPIO(scl)) {
        snifferSetupError = "invalid SCL GPIO";
        return false;
    }

    // Callback state must stay in internal SRAM, even on PSRAM boards.
    auto destroyCapture = [](FrequencyCapture* state) {
        if (!state) return;
        state->~FrequencyCapture();
        heap_caps_free(state);
    };
    void* storage = heap_caps_malloc(sizeof(FrequencyCapture), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    std::unique_ptr<FrequencyCapture, decltype(destroyCapture)> capture(
        storage ? new (storage) FrequencyCapture{} : nullptr, destroyCapture);
    if (!capture) {
        snifferSetupError = "failed to allocate frequency capture state";
        return false;
    }
    capture->dma = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        frequencyDmaSymbols * sizeof(rmt_symbol_word_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA));
    capture->samples = static_cast<rmt_symbol_word_t*>(heap_caps_malloc(
        frequencySampleSymbols * sizeof(rmt_symbol_word_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!capture->dma || !capture->samples) {
        snifferSetupError = "failed to allocate frequency capture buffers";
        return false;
    }

    // Wire must already be released by the caller. The observed bus supplies
    // its own pull-ups; this command never drives SCL or generates traffic.
    pinMode(scl, INPUT);
    rmt_rx_channel_config_t config = {};
    config.gpio_num = static_cast<gpio_num_t>(scl);
    config.clk_src = RMT_CLK_SRC_DEFAULT;
    config.resolution_hz = frequencyResolutionHz;
    config.mem_block_symbols = frequencyDmaSymbols;
    config.flags.with_dma = true;

    auto check = [this](esp_err_t error) {
        if (error == ESP_OK) return true;
        snifferSetupError = esp_err_to_name(error);
        return false;
    };
    if (!check(rmt_new_rx_channel(&config, &capture->channel))) return false;
    rmt_rx_event_callbacks_t callbacks = {};
    callbacks.on_recv_done = onFrequencyReceived;
    if (!check(rmt_rx_register_event_callbacks(capture->channel, &callbacks, capture.get()))) return false;
    if (!check(rmt_enable(capture->channel))) return false;
    capture->enabled = true;
    capture->receiveConfig.signal_range_min_ns = 100;
    capture->receiveConfig.signal_range_max_ns = 1000000; // 1 ms idle, fits 15-bit ticks
    capture->receiveConfig.flags.en_partial_rx = true;
    __atomic_store_n(&capture->running, true, __ATOMIC_RELEASE);
    if (!check(rmt_receive(capture->channel, capture->dma,
            frequencyDmaSymbols * sizeof(rmt_symbol_word_t), &capture->receiveConfig))) return false;

    const uint32_t started = millis();
    I2cFrequencyAnalyzer analyzer;
    size_t processed = 0;
    while (timeoutMs == 0 || static_cast<uint32_t>(millis() - started) < timeoutMs) {
        if (shouldStop && shouldStop()) break;
        if (__atomic_load_n(&capture->receiveError, __ATOMIC_ACQUIRE) != ESP_OK) break;

        // Published samples are immutable; the ISR only appends after this
        // prefix. Keep collecting sparse bursts until enough clocks agree.
        const size_t count = __atomic_load_n(&capture->count, __ATOMIC_ACQUIRE);
        if (count != processed) {
            for (; processed < count; ++processed) {
                const auto& symbol = capture->samples[processed];
                analyzer.addPulse(symbol.duration0, symbol.level0 != 0);
                if (symbol.duration0 != 0) analyzer.addPulse(symbol.duration1, symbol.level1 != 0);
            }
            result = analyzer.result(frequencyResolutionHz);
            if (result.reliable) break;
        }

        if (count == frequencySampleSymbols) {
            // Noise or changing clocks filled this bounded window. Start a
            // fresh window so an earlier bad capture cannot poison detection.
            capture->stop();
            analyzer = I2cFrequencyAnalyzer{};
            processed = 0;
            __atomic_store_n(&capture->count, 0u, __ATOMIC_RELEASE);
            if (!check(rmt_enable(capture->channel))) return false;
            capture->enabled = true;
            __atomic_store_n(&capture->running, true, __ATOMIC_RELEASE);
            if (!check(rmt_receive(capture->channel, capture->dma,
                    frequencyDmaSymbols * sizeof(rmt_symbol_word_t), &capture->receiveConfig))) return false;
        }
        delay(1);
    }
    capture->stop();
    if (!check(__atomic_load_n(&capture->receiveError, __ATOMIC_ACQUIRE))) return false;

    return true;
#else
    snifferSetupError = "SCL frequency capture requires RMT RX DMA support";
    return false;
#endif
}

#if BITPIRATE_I2C_SNIFFER_OPT_O3 && defined(__GNUC__)
#pragma GCC pop_options
#endif
