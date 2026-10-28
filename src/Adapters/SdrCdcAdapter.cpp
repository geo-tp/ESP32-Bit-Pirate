#include "SdrCdcAdapter.h"

#include "Services/SdrCaptureService.h"
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
namespace SdrDsp {
constexpr size_t bins = 256;
constexpr float pi = 3.14159265358979323846f;

inline void fft256(float* values) {
    for (size_t i = 1, reversed = 0; i < bins; ++i) {
        size_t bit = bins / 2;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) {
            for (size_t component = 0; component < 2; ++component) {
                const float saved = values[2 * i + component];
                values[2 * i + component] = values[2 * reversed + component];
                values[2 * reversed + component] = saved;
            }
        }
    }
    for (size_t width = 2; width <= bins; width *= 2) {
        const float angle = -2.0f * pi / width;
        const float stepReal = std::cos(angle), stepImag = std::sin(angle);
        for (size_t base = 0; base < bins; base += width) {
            float real = 1.0f, imag = 0.0f;
            for (size_t offset = 0; offset < width / 2; ++offset) {
                const size_t a = 2 * (base + offset), b = a + width;
                const float tr = real * values[b] - imag * values[b + 1];
                const float ti = real * values[b + 1] + imag * values[b];
                values[b] = values[a] - tr;
                values[b + 1] = values[a + 1] - ti;
                values[a] += tr;
                values[a + 1] += ti;
                const float nextReal = real * stepReal - imag * stepImag;
                imag = real * stepImag + imag * stepReal;
                real = nextReal;
            }
        }
    }
}

inline int signed10(unsigned value) {
    value &= 1023u;
    return value >= 512u ? static_cast<int>(value) - 1024 : static_cast<int>(value);
}
}

constexpr uint32_t internalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr size_t bandwidthHeaderBytes = 40;
constexpr size_t extendedHeaderBytes = 44;
static_assert(SdrDsp::bins == SdrCaptureService::FFT_SAMPLE_COUNT, "FFT/capture size mismatch");

enum class CommandKind { Invalid, Info, Memory, Help, Iq, Fft, Raw2, Stream2,
                         Tune, Cpu, RfState, Bandwidth, BandwidthQuery,
                         Rate, RateQuery, GainHardware, GainManual, GainQuery, Stop, Reboot };
// IQ frame flags bits 8..11 identify the payload format in every header version.
// Control/error frame flags retain their existing meaning.
enum class SampleFormat : uint8_t { Raw32 = 0, Iq10Packed = 1 };
struct Command {
    CommandKind kind = CommandKind::Invalid;
    uint16_t tuningValue = 0;
    uint16_t count = 0;
    SampleFormat sampleFormat = SampleFormat::Raw32;
};

size_t payloadBytes(SampleFormat format, size_t sampleCount) {
    return format == SampleFormat::Iq10Packed
        ? (sampleCount * 20 + 7) / 8 : sampleCount * sizeof(uint32_t);
}

bool space(char c) { return c == ' ' || c == '\t'; }

bool wordEquals(const char* word, size_t length, const char* expected) {
    if (length != std::strlen(expected)) return false;
    for (size_t i = 0; i < length; ++i) {
        const char upper = word[i] >= 'a' && word[i] <= 'z' ? word[i] - ('a' - 'A') : word[i];
        if (upper != expected[i]) return false;
    }
    return true;
}

bool parseNumber(const char*& text, uint32_t maximum, uint32_t& value) {
    if (*text < '0' || *text > '9') return false;
    value = 0;
    do {
        const uint32_t digit = static_cast<uint32_t>(*text++ - '0');
        if (digit > maximum || value > (maximum - digit) / 10) return false;
        value = value * 10 + digit;
    } while (*text >= '0' && *text <= '9');
    return true;
}

Command parseCommand(const char* text) {
    while (space(*text)) ++text;
    const char* word = text;
    while (*text && !space(*text)) ++text;
    const size_t length = text - word;
    while (space(*text)) ++text;
    if (!*text) {
        if (wordEquals(word, length, "INFO")) return {CommandKind::Info, 0};
        if (wordEquals(word, length, "MEM")) return {CommandKind::Memory, 0};
        if (wordEquals(word, length, "HELP") || wordEquals(word, length, "?")) return {CommandKind::Help, 0};
        if (wordEquals(word, length, "STOP")) return {CommandKind::Stop, 0};
        if (wordEquals(word, length, "RFSTATE")) return {CommandKind::RfState, 0};
        if (wordEquals(word, length, "BANDWIDTH?")) return {CommandKind::BandwidthQuery, 0};
        if (wordEquals(word, length, "RATE?")) return {CommandKind::RateQuery, 0};
        if (wordEquals(word, length, "GAIN?")) return {CommandKind::GainQuery, 0};
        if (wordEquals(word, length, "REBOOT")) return {CommandKind::Reboot, 0};
        return {};
    }

    if (wordEquals(word, length, "RATE")) {
        uint32_t mhz = 0;
        if (!parseNumber(text, 80, mhz)) return {};
        while (space(*text)) ++text;
        if (*text || (mhz != 16 && mhz != 40 && mhz != 80)) return {};
        return {CommandKind::Rate, static_cast<uint16_t>(mhz), 0};
    }
    if (wordEquals(word, length, "GAIN")) {
        const char* argument = text;
        while (*text && !space(*text)) ++text;
        const size_t argumentLength = text - argument;
        while (space(*text)) ++text;
        if (wordEquals(argument, argumentLength, "HARDWARE")) {
            if (*text) return {};
            return {CommandKind::GainHardware, 0, 0};
        }
        if (!wordEquals(argument, argumentLength, "MANUAL")) return {};
        uint32_t index = 0;
        if (!parseNumber(text, 82, index)) return {};
        while (space(*text)) ++text;
        if (*text) return {};
        return {CommandKind::GainManual, static_cast<uint16_t>(index), 0};
    }

    if (wordEquals(word, length, "BANDWIDTH")) {
        const char* argument = text;
        while (*text && !space(*text)) ++text;
        const size_t argumentLength = text - argument;
        while (space(*text)) ++text;
        if (*text) return {};
        if (wordEquals(argument, argumentLength, "AUTO")) return {CommandKind::Bandwidth, 0xffff};
        if (wordEquals(argument, argumentLength, "WIDE")) return {CommandKind::Bandwidth, 0};
        uint32_t mhz = 0;
        const char* number = argument;
        if (!parseNumber(number, SdrCaptureService::MAX_BANDWIDTH_MHZ, mhz) ||
            number != argument + argumentLength ||
            !SdrCaptureService::isValidBandwidth(int(mhz))) return {};
        return {CommandKind::Bandwidth, uint16_t(mhz)};
    }
    CommandKind kind = CommandKind::Invalid;
    if (wordEquals(word, length, "IQ")) kind = CommandKind::Iq;
    if (wordEquals(word, length, "FFT")) kind = CommandKind::Fft;
    if (wordEquals(word, length, "RAW2")) kind = CommandKind::Raw2;
    if (wordEquals(word, length, "STREAM2")) kind = CommandKind::Stream2;
    if (wordEquals(word, length, "TUNE")) kind = CommandKind::Tune;
    if (wordEquals(word, length, "CPU")) kind = CommandKind::Cpu;
    if (kind == CommandKind::Invalid) return {};
    uint32_t tuningValue = 0;
    if (!parseNumber(text, SdrCaptureService::MAX_CENTER_MHZ, tuningValue) || !tuningValue) return {};
    if (kind != CommandKind::Cpu && tuningValue < SdrCaptureService::MIN_CENTER_MHZ) return {};
    while (space(*text)) ++text;
    if (kind == CommandKind::Iq || kind == CommandKind::Fft ||
        kind == CommandKind::Tune || kind == CommandKind::Cpu) {
        if (*text) return {};
        return {kind, static_cast<uint16_t>(tuningValue), 0};
    }
    uint32_t count = 0;
    if (!parseNumber(text, SdrCaptureService::MAX_SAMPLE_COUNT, count) || !count) return {};
    if (*text && !space(*text)) return {};
    while (space(*text)) ++text;
    SampleFormat format = SampleFormat::Raw32;
    if (*text) {
        const char* argument = text;
        while (*text && !space(*text)) ++text;
        const size_t argumentLength = text - argument;
        if (wordEquals(argument, argumentLength, "IQ10_PACKED")) format = SampleFormat::Iq10Packed;
        else if (!wordEquals(argument, argumentLength, "RAW32")) return {};
    }
    while (space(*text)) ++text;
    if (*text) return {};
    return {kind, static_cast<uint16_t>(tuningValue), static_cast<uint16_t>(count), format};
}

class CommandReader {
public:
    enum class Event { None, Line, TooLong, InvalidByte, Timeout };

    Event push(char c, uint32_t now) {
        lastByte = now;
        if (c == '\r' || c == '\n') {
            if (discard) { discard = false; return Event::None; }
            if (!length) return Event::None;
            buffer[length] = '\0';
            length = 0;
            return Event::Line;
        }
        if (discard) return Event::None;
        if ((c < 32 && c != '\t') || static_cast<unsigned char>(c) > 126) {
            length = 0;
            discard = true;
            return Event::InvalidByte;
        }
        if (length == sizeof(buffer) - 1) {
            length = 0;
            discard = true;
            return Event::TooLong;
        }
        buffer[length++] = c;
        return Event::None;
    }

    Event expire(uint32_t now) {
        if (length && static_cast<uint32_t>(now - lastByte) >= 2000) {
            length = 0;
            discard = true;
            return Event::Timeout;
        }
        return Event::None;
    }

    const char* line() const { return buffer; }

private:
    char buffer[64] = {};
    size_t length = 0;
    uint32_t lastByte = 0;
    bool discard = false;
};

// Lives on the existing Arduino task stack only while this adapter is running.
class Session {
public:
    Session(IInput& input, IHostSerial& serial) : input(input), serial(serial) {}
    void run() {
        serial.disableReboot();
        serial.setRxBufferSize(512);
        serial.setTimeout(0);
        serial.begin(115200);
        esp_log_level_set("*", ESP_LOG_NONE);
        freeBeforeRadio = heap_caps_get_free_size(internalCaps);
        initError = radio.begin();
        freeAfterRadio = heap_caps_get_free_size(internalCaps);
        // The host may attach after boot, so INFO always repeats readiness.
        reply("READY BPRF1 ready=%u init_error=%s\n", radio.isReady(), esp_err_to_name(initError));
        for (;;) {
            for (unsigned budget = 0; budget < 64 && serial.available() > 0; ++budget) {
                const int c = serial.read();
                if (c >= 0) handleEvent(reader.push(static_cast<char>(c), millis()));
            }
            handleEvent(reader.expire(millis()));
            pollButton();
            delay(1);
        }
    }

private:
    void pollButton() {
        if (static_cast<uint32_t>(millis() - lastButtonPoll) < 25) return;
        lastButtonPoll = millis();
        if (input.readChar() != KEY_NONE) ESP.restart();
    }

    bool reply(const char* format, ...) {
        char line[640];
        va_list args;
        va_start(args, format);
        const int length = vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(line)) return false;
        size_t offset = 0;
        uint32_t lastProgress = millis();
        while (offset < static_cast<size_t>(length)) {
            pollButton();
            const int room = serial.availableForWrite();
            if (room > 0) {
                const size_t remaining = std::min(static_cast<size_t>(length) - offset, static_cast<size_t>(room));
                const size_t written = serial.write(reinterpret_cast<const uint8_t*>(line) + offset, remaining);
                if (written) { offset += written; lastProgress = millis(); }
            }
            if (static_cast<uint32_t>(millis() - lastProgress) >= 1000) return false;
            if (offset < static_cast<size_t>(length)) delay(1);
        }
        return true;
    }

    void handleEvent(CommandReader::Event event) {
        using Event = CommandReader::Event;
        switch (event) {
            case Event::None: return;
            case Event::TooLong: reply("ERR COMMAND_TOO_LONG\n"); return;
            case Event::InvalidByte: reply("ERR INVALID_BYTE\n"); return;
            case Event::Timeout: reply("ERR COMMAND_TIMEOUT\n"); return;
            case Event::Line: break;
        }
        const auto command = parseCommand(reader.line());
        using Kind = CommandKind;
        switch (command.kind) {
            case Kind::Info:
                reply("INFO BPRF1 ready=%u init_error=%s idf=%s samples=256 max_samples=%u fs_hz=%u "
                        "rates_hz=80000000,40000000,16000000 "
                        "center_min_mhz=%u center_max_mhz=%u "
                        "center_step_mhz=1 experimental_tuning=1 recommended_band=2.4GHz "
                        "reserved_bytes=65536 captures=%u failures=%u "
                        "bw_control=1 bw_min_mhz=13 bw_max_mhz=69 bw_requested_mhz=%d "
                        "gain_control=%u gain_max=%u frame_versions=2,3 "
                        "sample_formats=RAW32,IQ10_PACKED default_sample_format=RAW32\nEND\n",
                      radio.isReady(), esp_err_to_name(initError), esp_get_idf_version(),
                        unsigned(SdrCaptureService::MAX_SAMPLE_COUNT), unsigned(radio.sampleRate()),
                        unsigned(SdrCaptureService::MIN_CENTER_MHZ),
                        unsigned(SdrCaptureService::MAX_CENTER_MHZ), captures, failures, radio.bandwidthMHz(),
                        unsigned(radio.manualGainSupported()), radio.maxGainIndex());
                break;
            case Kind::Memory: memory(); break;
            case Kind::Help:
                reply("HELP BPRF1\nINFO\nMEM\nIQ/FFT <center_MHz 100..6000> (256 samples)\n"
                      "RAW2/STREAM2 <center_MHz 100..6000> <samples 1..16380> [RAW32|IQ10_PACKED]\n"
                      "Experimental 1 MHz steps; recommended RF region: 2.4 GHz. RAW32 is default.\n"
                      "Headers: v2=40 B +BW; above 4294 MHz: v3=44 B +BW +center Hz high32.\n"
                      "IQ flags: bit0=RX change; bits8..11=format (0=RAW32,1=IQ10_PACKED).\n"
                      "STOP\nREBOOT\nTUNE <center_MHz> (while streaming)\nRFSTATE\nCPU <80|160|240>\n"
                      "BANDWIDTH <13..69|0|WIDE|AUTO>\nBANDWIDTH?\n"
                      "RATE <80|40|16> (nominal MS/s)\nRATE?\n"
                      "GAIN HARDWARE\nGAIN MANUAL <PHY index>\nGAIN?\n"
                      "END\n");
                break;
            case Kind::Iq: snapshot(command.tuningValue, true); break;
            case Kind::Fft: snapshot(command.tuningValue, false); break;
            case Kind::Raw2:
                sampleFormat = command.sampleFormat;
                binaryCapture(command.tuningValue, command.count, false); break;
            case Kind::Stream2:
                sampleFormat = command.sampleFormat;
                stream(command.tuningValue, command.count); break;
            case Kind::Stop: reply("ERR STOP_NOT_STREAMING\n"); break;
            case Kind::Tune: reply("ERR TUNE_NOT_STREAMING\n"); break;
            case Kind::Cpu: setCpu(command.tuningValue); break;
            case Kind::RfState: rfState(); break;
            case Kind::Bandwidth:
                radio.setBandwidth(command.tuningValue == 0xffff
                    ? SdrCaptureService::AUTO_BANDWIDTH : command.tuningValue);
                bandwidthState(); break;
            case Kind::BandwidthQuery: bandwidthState(); break;
            case Kind::Rate:
                if (!radio.setSampleRate(uint32_t(command.tuningValue) * 1000000u))
                    reply("ERR RATE use_80_40_16\n");
                else
                    rateState();
                break;
            case Kind::RateQuery: rateState(); break;
            case Kind::GainHardware:
                if (!radio.setHardwareGain()) reply("ERR GAIN_UNAVAILABLE\n");
                else gainState();
                break;
            case Kind::GainManual:
                if (!radio.setManualGain(command.tuningValue)) reply("ERR GAIN_UNAVAILABLE_OR_RANGE\n");
                else gainState();
                break;
            case Kind::GainQuery: gainState(); break;
            case Kind::Reboot:
                reply("OK REBOOT\nEND\n");
                delay(50);
                ESP.restart();
                break;
            default: reply("ERR COMMAND use_INFO_MEM_IQ_FFT_RAW2_STREAM2_TUNE_STOP_RFSTATE_BANDWIDTH_RATE_GAIN_CPU_HELP_REBOOT\n"); break;
        }
    }

    bool writeBytes(const uint8_t* data, size_t length) {
        size_t offset = 0;
        uint32_t lastProgress = millis();
        while (offset < length) {
            pollButton();
            const int room = serial.availableForWrite();
            if (room > 0) {
                const size_t chunk = std::min(length - offset,
                    std::min(static_cast<size_t>(room), static_cast<size_t>(4096)));
                const size_t written = serial.write(data + offset, chunk);
                if (written) {
                    offset += written;
                    lastProgress = millis();
                }
            }
            if (static_cast<uint32_t>(millis() - lastProgress) >= 1000) return false;
            if (offset < length) delay(0);
        }
        return true;
    }

    bool writePackedIq(const volatile uint32_t* words, size_t sampleCount) {
        // 128 samples per chunk; complete pairs avoid padding between chunks.
        // The capture bank stays untouched and no heap workspace is allocated.
        constexpr size_t chunkSamples = 128;
        uint8_t packed[chunkSamples * 5 / 2];
        while (sampleCount) {
            const size_t count = std::min(sampleCount, chunkSamples);
            size_t sample = 0, bytes = 0;
            for (; sample + 1 < count; sample += 2) {
                const uint32_t a = words[sample] & 0xfffffu;
                const uint32_t b = words[sample + 1] & 0xfffffu;
                // Little-endian bitstream: I0[10], Q0[10], I1[10], Q1[10].
                packed[bytes++] = static_cast<uint8_t>(a);
                packed[bytes++] = static_cast<uint8_t>(a >> 8);
                packed[bytes++] = static_cast<uint8_t>((a >> 16) | (b << 4));
                packed[bytes++] = static_cast<uint8_t>(b >> 4);
                packed[bytes++] = static_cast<uint8_t>(b >> 12);
            }
            if (sample < count) {
                const uint32_t a = words[sample] & 0xfffffu;
                packed[bytes++] = static_cast<uint8_t>(a);
                packed[bytes++] = static_cast<uint8_t>(a >> 8);
                packed[bytes++] = static_cast<uint8_t>(a >> 16); // High nibble is zero.
            }
            // Finish even short USB writes before reusing the temporary buffer.
            if (!writeBytes(packed, bytes)) return false;
            words += count;
            sampleCount -= count;
        }
        return true;
    }

    static void putU16(uint8_t* destination, uint16_t value) {
        destination[0] = static_cast<uint8_t>(value);
        destination[1] = static_cast<uint8_t>(value >> 8);
    }

    static void putU32(uint8_t* destination, uint32_t value) {
        destination[0] = static_cast<uint8_t>(value);
        destination[1] = static_cast<uint8_t>(value >> 8);
        destination[2] = static_cast<uint8_t>(value >> 16);
        destination[3] = static_cast<uint8_t>(value >> 24);
    }

    size_t appendMetadata(uint8_t* header, uint16_t centerFrequencyMHz,
                          SdrCaptureService::BandwidthReading reading = {}) const {
        // Hz exceeds uint32_t above 4294 MHz. V3 extends V2 with the high
        // 32 bits at offset 40, for IQ, error and end frames alike.
        const uint64_t centerHz = uint64_t(centerFrequencyMHz) * 1000000u;
        const uint32_t centerHigh = static_cast<uint32_t>(centerHz >> 32);
        putU32(header + 12, static_cast<uint32_t>(centerHz));
        header[4] = centerHigh ? 3 : 2;
        putU16(header + 32, static_cast<uint16_t>(radio.bandwidthMHz()));
        header[34] = reading.iCode;
        header[35] = reading.qCode;
        putU32(header + 36, reading.estimatedHz);
        if (centerHigh) {
            putU32(header + 40, centerHigh);
            return extendedHeaderBytes;
        }
        return bandwidthHeaderBytes;
    }

    void writeControlFrame(uint8_t type, uint16_t flags, uint16_t centerFrequencyMHz,
                           uint16_t sampleCount, uint32_t captureUs) {
        uint8_t header[extendedHeaderBytes] = {'B', 'P', 'R', 'F', 2, type};
        putU16(header + 6, flags);
        putU32(header + 8, captures);
        putU32(header + 16, radio.sampleRate());
        putU32(header + 20, sampleCount);
        putU32(header + 24, captureUs);
        putU32(header + 28, 0);
        writeBytes(header, appendMetadata(header, centerFrequencyMHz));
    }

    bool binaryCapture(uint16_t centerFrequencyMHz, uint16_t sampleCount, bool streamMode) {
        if (!radio.isReady()) {
            if (streamMode) writeControlFrame(3, 1, centerFrequencyMHz, sampleCount, 0);
            else reply("ERR RADIO_INIT %s\n", esp_err_to_name(initError));
            return false;
        }
        const auto capture = radio.capture(centerFrequencyMHz, sampleCount);
        if (capture.status != SdrCaptureService::Status::Ok) {
            ++failures;
            uint16_t errorCode = 1;
            const char* reason = "NOT_READY";
            switch (capture.status) {
                case SdrCaptureService::Status::Frequency: errorCode = 2; reason = "FREQUENCY"; break;
                case SdrCaptureService::Status::Count: errorCode = 3; reason = "SAMPLE_COUNT"; break;
                case SdrCaptureService::Status::EngineBusy: errorCode = 4; reason = "ENGINE_BUSY"; break;
                case SdrCaptureService::Status::Timeout: errorCode = 5; reason = "CAPTURE_TIMEOUT"; break;
                case SdrCaptureService::Status::Unchanged: errorCode = 6; reason = "UNCHANGED_SAMPLES"; break;
                case SdrCaptureService::Status::Filter: errorCode = 10; reason = "RX_FILTER_READBACK"; break;
                default: break;
            }
            if (streamMode) writeControlFrame(3, errorCode, centerFrequencyMHz, sampleCount, capture.elapsedUs);
            else reply("ERR %s driver=%s control=0x%08lx capture_us=%lu\n", reason,
                       esp_err_to_name(capture.error), static_cast<unsigned long>(capture.control),
                       static_cast<unsigned long>(capture.elapsedUs));
            return false;
        }

        ++captures;
        uint8_t header[extendedHeaderBytes] = {'B', 'P', 'R', 'F', 2, 1};
        putU16(header + 6, (capture.retuned ? 1 : 0) | (static_cast<uint16_t>(sampleFormat) << 8));
        putU32(header + 8, captures);
        putU32(header + 16, radio.sampleRate());
        putU32(header + 20, sampleCount);
        putU32(header + 24, capture.elapsedUs);
        putU32(header + 28, payloadBytes(sampleFormat, sampleCount));
        if (!writeBytes(header, appendMetadata(header, centerFrequencyMHz, capture.bandwidth))) return false;

        const auto* words = radio.samples();
        if (sampleFormat == SampleFormat::Iq10Packed) return writePackedIq(words, sampleCount);
        const auto* payload = reinterpret_cast<const uint8_t*>(const_cast<uint32_t*>(words));
        return writeBytes(payload, sampleCount * sizeof(uint32_t));
    }

    void stream(uint16_t centerFrequencyMHz, uint16_t sampleCount) {
        bool stopped = false;
        while (!stopped) {
            if (!processStreamControl(centerFrequencyMHz, sampleCount)) stopped = true;
            if (!stopped && !binaryCapture(centerFrequencyMHz, sampleCount, true)) break;
        }
        writeControlFrame(2, 0, centerFrequencyMHz, 0, 0);
    }

    bool processStreamControl(uint16_t& centerFrequencyMHz, uint16_t sampleCount) {
        for (unsigned budget = 0; budget < 64 && serial.available() > 0; ++budget) {
            const int character = serial.read();
            if (character < 0) break;
            const auto event = reader.push(static_cast<char>(character), millis());
            if (event == CommandReader::Event::Line) {
                const auto command = parseCommand(reader.line());
                if (command.kind == CommandKind::Stop) return false;
                if (command.kind == CommandKind::Tune) {
                    centerFrequencyMHz = command.tuningValue;
                    continue;
                }
                // The next IQ header acknowledges the new filter setting.
                if (command.kind == CommandKind::Bandwidth) {
                    radio.setBandwidth(command.tuningValue == 0xffff
                        ? SdrCaptureService::AUTO_BANDWIDTH : command.tuningValue);
                    continue;
                }
                if (command.kind == CommandKind::Rate) {
                    if (radio.setSampleRate(uint32_t(command.tuningValue) * 1000000u)) continue;
                    writeControlFrame(3, 7, centerFrequencyMHz, sampleCount, 0);
                    continue;
                }
                if (command.kind == CommandKind::GainHardware) {
                    if (radio.setHardwareGain()) continue;
                    writeControlFrame(3, 7, centerFrequencyMHz, sampleCount, 0);
                    continue;
                }
                if (command.kind == CommandKind::GainManual) {
                    if (radio.setManualGain(command.tuningValue)) continue;
                    writeControlFrame(3, 7, centerFrequencyMHz, sampleCount, 0);
                    continue;
                }
                if (command.kind == CommandKind::Reboot) {
                    writeControlFrame(2, 0, centerFrequencyMHz, 0, 0);
                    reply("OK REBOOT\n");
                    delay(50);
                    ESP.restart();
                }
                writeControlFrame(3, 7, centerFrequencyMHz, sampleCount, 0);
            } else if (event == CommandReader::Event::TooLong) {
                writeControlFrame(3, 7, centerFrequencyMHz, sampleCount, 0);
            } else if (event == CommandReader::Event::InvalidByte) {
                writeControlFrame(3, 8, centerFrequencyMHz, sampleCount, 0);
            } else if (event == CommandReader::Event::Timeout) {
                writeControlFrame(3, 9, centerFrequencyMHz, sampleCount, 0);
            }
        }
        if (reader.expire(millis()) == CommandReader::Event::Timeout)
            writeControlFrame(3, 9, centerFrequencyMHz, sampleCount, 0);
        return true;
    }

    void rfState() {
        reply("RFSTATE ready=%u center_mhz=%u wifi_channel=%u retunes=%u last_retune_us=%u cpu_mhz=%u apb_hz=%u "
              "fs_hz=%u bw_requested_mhz=%d bw_estimated_hz=%u gain_mode=%s gain_index=%d gain_max=%u\nEND\n",
              radio.isReady(), unsigned(radio.activeFrequency()), unsigned(radio.wifiChannel()),
              unsigned(radio.retuneCount()), unsigned(radio.lastRetuneUs()),
              unsigned(getCpuFrequencyMhz()), unsigned(getApbFrequency()), unsigned(radio.sampleRate()),
              radio.bandwidthMHz(), unsigned(radio.lastBandwidth().estimatedHz),
              radio.manualGain() ? "MANUAL" : "HARDWARE",
              radio.manualGain() ? int(radio.gainIndex()) : -1, radio.maxGainIndex());
    }

    void bandwidthState() {
        const auto reading = radio.lastBandwidth();
        const uint32_t targetHz = radio.targetBandwidthHz();
        reply("BANDWIDTH requested_mhz=%d target_hz=%u estimated_hz=%u dcap_i=%u dcap_q=%u "
              "actual_valid=%u approximate=1\nEND\n",
              radio.bandwidthMHz(), unsigned(targetHz), unsigned(reading.estimatedHz),
              unsigned(reading.iCode), unsigned(reading.qCode),
              unsigned(reading.iCode != SdrCaptureService::UNKNOWN_BANDWIDTH_CODE &&
                       reading.qCode != SdrCaptureService::UNKNOWN_BANDWIDTH_CODE));
    }

    void rateState() {
        reply("RATE fs_hz=%u supported_hz=80000000,40000000,16000000 nominal=1\nEND\n",
              unsigned(radio.sampleRate()));
    }

    void gainState() {
        reply("GAIN supported=%u mode=%s index=%d max=%u calibrated_db=0\nEND\n",
              unsigned(radio.manualGainSupported()),
              radio.manualGain() ? "MANUAL" : "HARDWARE",
              radio.manualGain() ? int(radio.gainIndex()) : -1,
              radio.maxGainIndex());
    }

    // 80 MHz minimum: Wi-Fi needs it, and APB stays at 80 MHz so USB is unaffected.
    void setCpu(uint16_t mhz) {
        if (mhz != 80 && mhz != 160 && mhz != 240) { reply("ERR CPU_MHZ use_80_160_240\n"); return; }
        if (!setCpuFrequencyMhz(mhz)) { reply("ERR CPU_SET\n"); return; }
        reply("OK CPU mhz=%u apb_hz=%u\nEND\n", unsigned(getCpuFrequencyMhz()), unsigned(getApbFrequency()));
    }

    void memory() {
        const bool intact = heap_caps_check_integrity(internalCaps, false);
        reply("MEM internal_free=%u largest=%u minimum=%u dma_free=%u psram_free=%u "
              "stack_min_bytes=%u before_radio=%u after_radio=%u reserved_bytes=65536 heap_ok=%u\nEND\n",
              unsigned(heap_caps_get_free_size(internalCaps)),
              unsigned(heap_caps_get_largest_free_block(internalCaps)),
              unsigned(heap_caps_get_minimum_free_size(internalCaps)),
              unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)),
              unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
              unsigned(uxTaskGetStackHighWaterMark(nullptr)), freeBeforeRadio, freeAfterRadio, intact);
    }

    void snapshot(uint16_t centerFrequencyMHz, bool raw) {
        if (!radio.isReady()) {
            reply("ERR RADIO_INIT %s\n", esp_err_to_name(initError));
            return;
        }
        const uint32_t heapBefore = heap_caps_get_free_size(internalCaps);
        std::unique_ptr<float, decltype(&heap_caps_free)> data(
            raw ? nullptr : static_cast<float*>(heap_caps_malloc(2 * SdrDsp::bins * sizeof(float), internalCaps)),
            heap_caps_free);
        if (!raw && !data) { reply("ERR FFT_MEMORY\n"); return; }
        const uint32_t heapDuring = heap_caps_get_free_size(internalCaps);
        const auto capture = radio.capture(centerFrequencyMHz, SdrCaptureService::FFT_SAMPLE_COUNT);
        if (capture.status != SdrCaptureService::Status::Ok) {
            ++failures;
            const char* reason = "NOT_READY";
            switch (capture.status) {
                case SdrCaptureService::Status::Frequency: reason = "FREQUENCY"; break;
                case SdrCaptureService::Status::Count: reason = "SAMPLE_COUNT"; break;
                case SdrCaptureService::Status::EngineBusy: reason = "ENGINE_BUSY"; break;
                case SdrCaptureService::Status::Timeout: reason = "CAPTURE_TIMEOUT"; break;
                case SdrCaptureService::Status::Unchanged: reason = "UNCHANGED_SAMPLES"; break;
                case SdrCaptureService::Status::Filter: reason = "RX_FILTER_READBACK"; break;
                default: break;
            }
            reply("ERR %s driver=%s control=0x%08lx capture_us=%lu\n", reason,
                  esp_err_to_name(capture.error), static_cast<unsigned long>(capture.control),
                  static_cast<unsigned long>(capture.elapsedUs));
            return;
        }
        ++captures;
        const auto* words = radio.samples();
        uint32_t fftUs = 0;
        if (!raw) {
            for (size_t i = 0; i < SdrDsp::bins; ++i) {
                const uint32_t word = words[i];
                const float window = 0.5f - 0.5f * std::cos(2.0f * SdrDsp::pi * i / SdrDsp::bins);
                data.get()[2 * i] = SdrDsp::signed10(word) * window;
                data.get()[2 * i + 1] = SdrDsp::signed10(word >> 10) * window;
            }
            const int64_t started = esp_timer_get_time();
            SdrDsp::fft256(data.get());
            fftUs = static_cast<uint32_t>(esp_timer_get_time() - started);
        }
        if (!reply("%s center_hz=%llu fs_hz=%u count=256 capture_us=%u fft_us=%u "
                   "heap_before=%u heap_during=%u capture_id=%u "
                   "bw_requested_mhz=%d bw_estimated_hz=%u dcap_i=%u dcap_q=%u\n",
                   raw ? "IQ" : "FFT", static_cast<unsigned long long>(centerFrequencyMHz) * 1000000u,
                   unsigned(radio.sampleRate()), unsigned(capture.elapsedUs), unsigned(fftUs),
                   heapBefore, heapDuring, captures,
                   radio.bandwidthMHz(), unsigned(capture.bandwidth.estimatedHz),
                   unsigned(capture.bandwidth.iCode), unsigned(capture.bandwidth.qCode))) return;
        for (size_t out = 0; out < SdrDsp::bins; ++out) {
            if (raw) {
                const uint32_t word = words[out];
                if (!reply("%d,%d\n", SdrDsp::signed10(word), SdrDsp::signed10(word >> 10))) return;
            } else {
                const size_t bin = (out + SdrDsp::bins / 2) % SdrDsp::bins;
                const float re = data.get()[2 * bin], im = data.get()[2 * bin + 1];
                constexpr float normalization = 512.0f * 512.0f * 128.0f * 128.0f;
                const float db = 10.0f * std::log10((re * re + im * im) / normalization + 1e-20f);
                const long offset = (static_cast<long>(out) - 128) *
                                    static_cast<long>(radio.sampleRate() / SdrDsp::bins);
                if (!reply("%ld,%.2f\n", offset, db)) return;
            }
        }
        reply("END\n");
        // unique_ptr frees the FFT workspace on success and USB timeout alike.
    }

    IInput& input;
    IHostSerial& serial;
    SdrCaptureService radio;
    CommandReader reader;
    esp_err_t initError = ESP_ERR_INVALID_STATE;
    uint32_t freeBeforeRadio = 0, freeAfterRadio = 0;
    uint32_t lastButtonPoll = 0, captures = 0, failures = 0;
    SampleFormat sampleFormat = SampleFormat::Raw32;
};
}

void SdrCdcAdapter::run(IInput& input, IHostSerial& hostSerial) {
    Session session(input, hostSerial);
    session.run();
}
