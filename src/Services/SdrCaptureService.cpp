// SPDX-License-Identifier: MIT
#include "SdrCaptureService.h"

#include <esp_bt.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <soc/soc.h>

#if !CONFIG_IDF_TARGET_ESP32S3
#error The SDR adapter requires ESP32-S3.
#endif

extern "C" {
void stop_tx_tone(unsigned mode);
void set_chanfreq(unsigned mhz, unsigned second);
void set_rf_freq_offset(unsigned crystal, unsigned mhz, int offsetKhz);
// Internal ESP32-S3 PHY API present in PioArduino's libphy.a (phy_api.o).
// The public headers do not expose a prototype, but disassembly shows two
// arguments and both are consumed as 8-bit values.
void phy_force_rx_gain(uint8_t enabled, uint8_t index);
void rom_pbus_workmode(void);
void rom_pbus_xpd_rx_on(unsigned enabled);
void rom_pbus_xpd_tx_off(void);
void rom_set_rxclk_en(unsigned enabled);
unsigned rom_chip_i2c_readReg(unsigned block, unsigned host, unsigned reg);
void rom_chip_i2c_writeReg(unsigned block, unsigned host, unsigned reg, unsigned value);
}

namespace {
constexpr uintptr_t bankStart = BootMemoryManager::SDR_BANK_START;

// Capture engine registers and control bits.
constexpr uintptr_t ownershipRegister = 0x600c101c;
constexpr uintptr_t captureRegister = 0x60033d5c;
constexpr uintptr_t packingRegister = 0x60033d90;
constexpr uint32_t enableCapture = 1u << 31;
constexpr uint32_t startCapture = 1u << 19;
constexpr uint32_t captureDone = 1u << 18;
constexpr uint32_t wordPacking = (1u << 6) | (2u << 12) | (3u << 18);
constexpr uint32_t rate40Bit = 1u << 15;
constexpr uint32_t rate16Bit = 1u << 16;
constexpr uint32_t sentinel = 0xd35a96c7;

constexpr uintptr_t gainControlRegister = 0x6001c02c; // AGCPWR_CTRL7 on S3.

// ESP32-S3 BBTOP capacitor measurements from ESPARGOS/esp-sdr rx_bandwidth.h:
// https://github.com/ESPARGOS/esp-sdr/blob/ac627b0b7cb1b31da6e41e08ee38e9ae9e21863e/main/common/rx_bandwidth.h
// Approximate full noise widths, not calibrated RF or alias-free bandwidths.
struct BandwidthPoint {
    uint8_t code;
    uint8_t mhz;
};

constexpr BandwidthPoint bandwidthCurve[] = {
    {0, 69}, {4, 51}, {8, 45}, {16, 33},
    {24, 25}, {32, 21}, {48, 16}, {60, 13}
};

// Returns -1 for AUTO/invalid; 0 is the widest filter, not filter bypass.
int bandwidthCapacitorCode(int mhz) {
    if (!SdrCaptureService::isValidBandwidth(mhz) || mhz == SdrCaptureService::AUTO_BANDWIDTH) {
        return -1;
    }
    if (mhz == 0) return 0;

    for (size_t i = 1; i < sizeof(bandwidthCurve) / sizeof(bandwidthCurve[0]); ++i) {
        const auto wide = bandwidthCurve[i - 1];
        const auto narrow = bandwidthCurve[i];
        if (mhz < narrow.mhz) continue;

        const unsigned width = wide.mhz - narrow.mhz;
        const unsigned distance = (wide.mhz - mhz) * (narrow.code - wide.code);
        return wide.code + (distance + width / 2) / width;
    }
    return -1;
}

uint32_t estimateBandwidthHz(unsigned code) {
    for (size_t i = 1; i < sizeof(bandwidthCurve) / sizeof(bandwidthCurve[0]); ++i) {
        const auto wide = bandwidthCurve[i - 1];
        const auto narrow = bandwidthCurve[i];
        if (code > narrow.code) continue;

        return uint32_t(wide.mhz) * 1000000u -
            (code - wide.code) * uint32_t(wide.mhz - narrow.mhz) * 1000000u /
            (narrow.code - wide.code);
    }
    return 0;
}

SdrCaptureService::BandwidthReading bandwidthReading(unsigned iCode, unsigned qCode) {
    const uint32_t iHz = estimateBandwidthHz(iCode);
    const uint32_t qHz = estimateBandwidthHz(qCode);
    // For independently calibrated I/Q filters, report the narrower estimate.
    return {uint8_t(iCode), uint8_t(qCode), iHz < qHz ? iHz : qHz};
}

// S3's ROM API uses host argument 0 for BBTOP, as in the S3 reference backend.
// This is the internal analog bus, unrelated to the user's I2C pins.
class ScopedRxFilter {
public:
    explicit ScopedRxFilter(int mhz) : code(bandwidthCapacitorCode(mhz)) {
        for (unsigned i = 0; i < 2; ++i) {
            saved[i] = rom_chip_i2c_readReg(0x67, 0, 4 + i);
        }
        if (code >= 0) {
            for (unsigned i = 0; i < 2; ++i) {
                rom_chip_i2c_writeReg(0x67, 0, 4 + i, (saved[i] & ~63u) | unsigned(code));
            }
        }

        const unsigned iCode = rom_chip_i2c_readReg(0x67, 0, 4) & 63u;
        const unsigned qCode = rom_chip_i2c_readReg(0x67, 0, 5) & 63u;
        value = bandwidthReading(iCode, qCode);
        matched = code < 0 || (iCode == unsigned(code) && qCode == unsigned(code));
    }

    ~ScopedRxFilter() {
        if (code >= 0) {
            for (unsigned i = 0; i < 2; ++i) {
                rom_chip_i2c_writeReg(0x67, 0, 4 + i, saved[i]);
            }
        }
    }

    ScopedRxFilter(const ScopedRxFilter&) = delete;
    ScopedRxFilter& operator=(const ScopedRxFilter&) = delete;

    bool ok() const { return matched; }
    SdrCaptureService::BandwidthReading reading() const { return value; }

private:
    int code;
    unsigned saved[2];
    bool matched;
    SdrCaptureService::BandwidthReading value;
};
} // namespace

// Radio initialization; boot memory ownership is managed independently.

esp_err_t SdrCaptureService::begin() {
    if (!BootMemoryManager::isSdrBankReserved()) return ESP_ERR_INVALID_STATE;
    if (ready) return ESP_OK;

    // This adapter boots before network services. Refuse live radio ownership
    // rather than tearing down another service's driver behind its back.
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) return ESP_ERR_INVALID_STATE;
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) return ESP_ERR_INVALID_STATE;

    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) return result; // Never erase the user's NVS.
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK) return result;

    bool started = false;
    if ((result = esp_wifi_set_storage(WIFI_STORAGE_RAM)) == ESP_OK &&
        (result = esp_wifi_set_mode(WIFI_MODE_NULL)) == ESP_OK &&
        (result = esp_wifi_start()) == ESP_OK) {
        started = true;
        if ((result = esp_wifi_set_ps(WIFI_PS_NONE)) == ESP_OK &&
            (result = esp_wifi_set_promiscuous(true)) == ESP_OK &&
            (result = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)) == ESP_OK) {
            wifiChannelSet = 1;
            ready = true;
            return ESP_OK;
        }
    }

    if (started) esp_wifi_stop();
    esp_wifi_deinit();
    return result;
}

// Receive configuration.

bool SdrCaptureService::setBandwidth(int mhz) {
    if (!isValidBandwidth(mhz)) return false;

    if (selectedBandwidthMHz != mhz) {
        selectedBandwidthMHz = mhz;
        lastBandwidthReading = {};
        retunePending = true;
    }
    return true;
}

uint32_t SdrCaptureService::targetBandwidthHz() const {
    const int code = bandwidthCapacitorCode(selectedBandwidthMHz);
    return code < 0 ? 0 : estimateBandwidthHz(unsigned(code));
}

bool SdrCaptureService::setSampleRate(uint32_t hz) {
    if (hz != 80000000u && hz != 40000000u && hz != 16000000u) return false;

    if (selectedSampleRate != hz) {
        selectedSampleRate = hz;
        retunePending = true;
    }
    return true;
}

bool SdrCaptureService::manualGainSupported() const {
    return maxGainIndex() != 0;
}

unsigned SdrCaptureService::maxGainIndex() const {
    // ESPARGOS reads AGCPWR_CTRL7 bits 8..14 and never exposes S3 indices >82.
    const unsigned maximum = (REG_READ(gainControlRegister) >> 8) & 127u;
    return maximum <= 82u ? maximum : 0u;
}

bool SdrCaptureService::setHardwareGain() {
    if (!gainManual) return true;

    gainManual = false;
    rxReconfigurePending = true;
    retunePending = true;
    return true;
}

bool SdrCaptureService::setManualGain(unsigned index) {
    if (!manualGainSupported() || index > maxGainIndex()) return false;

    if (!gainManual || selectedGainIndex != index) {
        gainManual = true;
        selectedGainIndex = index;
        rxReconfigurePending = true;
        retunePending = true;
    }
    return true;
}

bool SdrCaptureService::applyGain() {
    // Hardware AGC is the PHY default; this API either enables a forced PHY
    // table index or hands gain control back to the hardware AGC.
    unsigned index = selectedGainIndex;
    const unsigned maximum = maxGainIndex();
    if (maximum && index > maximum) index = maximum;
    phy_force_rx_gain(gainManual ? uint8_t(1) : uint8_t(0), static_cast<uint8_t>(index));
    return true;
}

// RF capture and sample access.

SdrCaptureService::Capture SdrCaptureService::capture(uint32_t centerFrequencyMHz, size_t sampleCount) {
    if (!ready || !BootMemoryManager::isSdrBankReserved()) return {Status::NotReady};
    if (centerFrequencyMHz < MIN_CENTER_MHZ || centerFrequencyMHz > MAX_CENTER_MHZ) {
        return {Status::Frequency, ESP_ERR_INVALID_ARG};
    }
    if (!sampleCount || sampleCount > MAX_SAMPLE_COUNT) return {Status::Count, ESP_ERR_INVALID_ARG};

    const uint32_t idleControl = REG_READ(captureRegister);
    if (idleControl != 0) return {Status::EngineBusy, ESP_OK, 0, idleControl};

    // Recalibrate and prepare RX only when the frequency or gain changed.
    if (activeFrequencyMHz != centerFrequencyMHz || rxReconfigurePending) {
        const int64_t tuneStart = esp_timer_get_time();
        const bool standardChannel = centerFrequencyMHz >= 2412 && centerFrequencyMHz <= 2472 &&
                                     (centerFrequencyMHz - 2412) % 5 == 0;
        const uint32_t wifiChannel = standardChannel
            ? 1 + (centerFrequencyMHz - 2412) / 5 : 1;
        if (wifiChannel != wifiChannelSet) {
            const esp_err_t tuning = esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
            if (tuning != ESP_OK) return {Status::Frequency, tuning};
            wifiChannelSet = wifiChannel;
        }

        if (standardChannel || centerFrequencyMHz == 2484) {
            set_chanfreq(centerFrequencyMHz, 0);
        } else {
            // Channel calibration cannot tune arbitrary MHz. Calibrate on
            // channel 1, then set the PLL directly (selector 0: 40 MHz crystal).
            set_chanfreq(2412, 0);
            set_rf_freq_offset(0, centerFrequencyMHz, 0);
        }

        stop_tx_tone(1);
        rom_pbus_workmode();
        rom_pbus_xpd_tx_off();
        rom_pbus_xpd_rx_on(1);
        rom_set_rxclk_en(1);
        if (!applyGain()) return {Status::NotReady, ESP_ERR_NOT_SUPPORTED};

        activeFrequencyMHz = centerFrequencyMHz;
        rxReconfigurePending = false;
        lastRetune = static_cast<uint32_t>(esp_timer_get_time() - tuneStart);
        ++retunes;
        retunePending = true;
    }

    auto* words = reinterpret_cast<volatile uint32_t*>(bankStart);
    const size_t middleSample = sampleCount / 2;
    const size_t lastSample = sampleCount - 1;
    words[0] = sentinel;
    words[middleSample] = sentinel;
    words[lastSample] = sentinel;

    // Retuning above always sees the calibrated register values. The guard
    // restores them on every return, including readback failure and timeout.
    lastBandwidthReading = {};
    ScopedRxFilter filter(selectedBandwidthMHz);
    if (!filter.ok()) return {Status::Filter, ESP_FAIL};

    const uint32_t owner = REG_READ(ownershipRegister);
    const uint32_t packing = REG_READ(packingRegister);
    REG_WRITE(packingRegister, wordPacking);
    REG_WRITE(ownershipRegister, (owner & ~0xfu) | 4u);

    // ESPARGOS S3 verified dump clocks: native 80 MS/s, bit15 halves to
    // nominal 40 MS/s, bit16 selects the nominal 16 MS/s hardware path.
    const uint32_t rateBits = selectedSampleRate == 40000000u ? rate40Bit :
                              selectedSampleRate == 16000000u ? rate16Bit : 0u;
    const uint32_t command = enableCapture | rateBits | static_cast<uint32_t>(sampleCount);
    const int64_t begin = esp_timer_get_time();
    REG_WRITE(captureRegister, command);
    REG_WRITE(captureRegister, command | startCapture);
    REG_WRITE(captureRegister, command);

    uint32_t control;
    do {
        control = REG_READ(captureRegister);
    } while (!(control & captureDone) && esp_timer_get_time() - begin < 20000);
    const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() - begin);

    // Restore the dump engine before checking the capture result.
    REG_WRITE(captureRegister, 0);
    REG_WRITE(ownershipRegister, owner);
    REG_WRITE(packingRegister, packing);

    if (!(control & captureDone)) return {Status::Timeout, ESP_OK, elapsed, control};
    if (words[0] == sentinel || words[middleSample] == sentinel || words[lastSample] == sentinel) {
        return {Status::Unchanged, ESP_OK, elapsed, control};
    }

    const bool retuned = retunePending;
    retunePending = false;
    lastBandwidthReading = filter.reading();
    return {Status::Ok, ESP_OK, elapsed, control, retuned, lastBandwidthReading};
}

const volatile uint32_t* SdrCaptureService::samples() const {
    return ready && BootMemoryManager::isSdrBankReserved() ? reinterpret_cast<const volatile uint32_t*>(bankStart) : nullptr;
}
