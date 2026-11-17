// Flipper parsing adapted from Ultimate Remote; see ../Vendors/UltimateRemote.LICENSE.
#include "InfraredRemoteTransformer.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <algorithm>
#include <string_view>
#include <numeric>
#include <limits>

namespace infrared_file {
std::string_view trim(std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    return begin == std::string::npos ? "" : text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

bool number(std::string_view text, uint32_t maximum, uint32_t& value, unsigned base = 10) {
    if (text.empty()) return false;
    value = 0;
    for (char c : text) {
        unsigned digit = c >= '0' && c <= '9' ? c - '0' :
                         c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                         c >= 'A' && c <= 'F' ? c - 'A' + 10 : base;
        if (digit >= base || digit > maximum || value > (maximum - digit) / base) return false;
        value = value * base + digit;
    }
    return true;
}

bool next(std::string_view& text, std::string_view& part, const char* separators) {
    if (text.empty()) return false;
    const auto end = text.find_first_of(separators);
    part = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    return true;
}

bool hex(std::string_view text, uint32_t& value) {
    std::string_view byte;
    value = 0;
    unsigned shift = 0;
    while (next(text, byte, " \t\r\n")) {
        if (byte.empty()) continue;
        uint32_t parsed;
        if (shift >= 32 || byte.size() != 2 || !number(byte, 255, parsed, 16)) return false;
        value |= parsed << shift;
        shift += 8;
    }
    // Accept the shorter fields written by older Bit Pirate versions, too.
    return shift > 0;
}
}

bool InfraredRemoteTransformer::isValidInfraredFile(const std::string& content) {
    if (content.empty() || content.find('\0') != std::string::npos) return false;
    std::string_view remaining(content);
    if (remaining.substr(0, 3) == "\xef\xbb\xbf") remaining.remove_prefix(3);
    std::string_view line;
    unsigned header = 0;
    while (infrared_file::next(remaining, line, "\n")) {
        line = infrared_file::trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (header == 0 && line != "Filetype: IR signals file" && line != "Filetype: IR") return false;
        if (header == 1) return line == "Version: 1";
        ++header;
    }
    return false;
}

std::vector<InfraredFileRemoteCommand> InfraredRemoteTransformer::transformFromFileFormat(const std::string& content) {
    std::vector<InfraredFileRemoteCommand> commands;
    if (!isValidInfraredFile(content)) return commands;
    bool legacyHeader = false;
    // Views refer to the input for this call only. Commands own their data.
    std::map<std::string_view, std::string_view> fields;
    auto finish = [&]() {
        if (fields["name"].empty()) return;
        InfraredFileRemoteCommand command;
        command.functionName = fields["name"];
        bool valid = false;
        if (fields["type"] == "raw") {
            command.protocol = _RAW;
            command.fileProtocol = "raw";
            valid = infrared_file::number(fields["frequency"], 1000000, command.frequency) && command.frequency >= 10000;
            char* end = nullptr;
            const auto duty = fields["duty_cycle"];
            // Views end at whitespace/newline or the input string's terminating NUL.
            command.dutyCycle = duty.empty() ? 0 : std::strtof(duty.data(), &end);
            valid = valid && !duty.empty() && end == duty.data() + duty.size() &&
                    std::isfinite(command.dutyCycle) && command.dutyCycle > 0 && command.dutyCycle <= 1;
            auto data = fields["data"];
            std::string_view duration;
            uint64_t total = 0;
            while (valid && infrared_file::next(data, duration, " \t\r\n")) {
                if (duration.empty()) continue;
                uint32_t value;
                valid = infrared_file::number(duration, 1000000, value) && value > 0;
                if (!valid) break;
                total += value;
                if (command.rawData.size() >= 65535 || total > 10000000) { valid = false; break; }
                command.rawData.push_back(value);
            }
            valid = valid && !command.rawData.empty();
        } else if (fields["type"] == "parsed") {
            command.fileProtocol = fields["protocol"];
            command.protocol = InfraredProtocolMapper::toEnum(command.fileProtocol);
            command.legacyDsf = legacyHeader && command.protocol != _IR_UNKNOWN &&
                command.fileProtocol == InfraredProtocolMapper::toString(command.protocol) &&
                !InfraredRemoteTransformer::isValidParsedCommand(command.fileProtocol, 0, 0);
            valid = infrared_file::hex(fields["address"], command.address) &&
                    infrared_file::hex(fields["command"], command.function) &&
                    (command.legacyDsf ? command.address <= 65535 && command.function <= 255 :
                     InfraredRemoteTransformer::isValidParsedCommand(command.fileProtocol, command.address, command.function));
        }
        if (!valid) {
            command.error = "Invalid or unsupported IR command";
            std::vector<uint32_t>().swap(command.rawData);
        }
        commands.push_back(std::move(command));
    };
    std::string_view remaining(content);
    if (remaining.substr(0, 3) == "\xef\xbb\xbf") remaining.remove_prefix(3);
    std::string_view line;
    bool header = true;
    while (infrared_file::next(remaining, line, "\n")) {
        line = infrared_file::trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (header) {
            legacyHeader = line == "Filetype: IR";
            header = false;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const auto key = infrared_file::trim(line.substr(0, colon));
        if (key == "name") { finish(); fields.clear(); }
        if (key == "name" || key == "type" || key == "protocol" || key == "address" ||
            key == "command" || key == "frequency" || key == "duty_cycle" || key == "data")
            fields[key] = infrared_file::trim(line.substr(colon + 1));
    }
    finish();
    return commands;
}

std::string InfraredRemoteTransformer::transformToFileFormat(const std::string& fileName, const std::vector<InfraredFileRemoteCommand>& commands) {
    std::ostringstream out;
    bool legacy = false;
    for (const auto& command : commands) legacy = legacy || command.legacyDsf;
    out << (legacy ? "Filetype: IR\n" : "Filetype: IR signals file\n");
    out << "Version: 1\n\n#\n# " << fileName << "\n#\n\n";
    for (const auto& command : commands) {
        out << "name: " << command.functionName << "\n";
        if (command.protocol == _RAW) {
            out << "type: raw\nfrequency: " << command.frequency << "\nduty_cycle: " << command.dutyCycle << "\ndata:";
            for (uint32_t duration : command.rawData) out << ' ' << duration;
            out << '\n';
        } else {
            out << "type: parsed\nprotocol: " << command.fileProtocol << '\n';
            out << "address: " << toHexBytesLE(command.address, 4) << '\n';
            out << "command: " << toHexBytesLE(command.function, 4) << '\n';
        }
        out << '\n';
    }
    return out.str();
}

bool InfraredRemoteTransformer::appendRawCommand(std::string& file, const std::string& name,
                                                 const std::vector<uint16_t>& timings,
                                                 uint32_t khz, size_t maxBytes) {
    maxBytes = std::min(maxBytes, MAX_FILE_SIZE);
    if (name.empty() || name.find_first_of("\r\n") != std::string::npos ||
        name.find('\0') != std::string::npos || timings.empty() || timings.size() > 65535 ||
        khz < 10 || khz > 1000 || file.size() > maxBytes) return false;
    const std::string header = "name: " + name + "\ntype: raw\nfrequency: " +
        std::to_string(khz * 1000) + "\nduty_cycle: 0.33\ndata:";
    size_t bytes = header.size() + 2; // Newline and separator after data.
    uint64_t total = 0;
    for (uint16_t timing : timings) {
        if (!timing) return false;
        total += timing;
        bytes += 1 + std::to_string(timing).size();
    }
    if (total > 10000000 || bytes > MAX_COMMAND_SIZE - 48 ||
        bytes > maxBytes - file.size()) return false;
    // Grow along subdivisions of the limit (e.g. 4/8/16/32 KiB). Simply reserving
    // maxBytes after arbitrary growth can make std::string double past the limit.
    const auto size = file.size() + bytes;
    if (file.capacity() < size) {
        size_t capacity = maxBytes;
        while (capacity / 2 >= size) capacity /= 2;
        file.reserve(capacity);
    }
    file += header;
    for (uint16_t timing : timings) {
        file += ' ';
        file += std::to_string(timing);
    }
    file += "\n\n";
    return true;
}

std::vector<std::string> InfraredRemoteTransformer::extractFunctionNames(const std::vector<InfraredFileRemoteCommand>& commands) {
    std::vector<std::string> names;
    names.reserve(commands.size());
    for (const auto& command : commands) names.push_back(command.functionName);
    return names;
}

std::string InfraredRemoteTransformer::toHexBytesLE(uint32_t value, size_t byteCount) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < byteCount; ++i) {
        if (i) out << ' ';
        out << std::setw(2) << ((value >> (i * 8)) & 0xFFu);
    }
    return out.str();
}


// Waveform generation adapted from Ultimate Remote.
// Adapted from ultimate-remote/src/Services/FlipperEncoder.cpp.
// License: ../Vendors/UltimateRemote.LICENSE.






bool InfraredRemoteTransformer::appendRawTiming(std::vector<uint16_t>& timings,
                                                bool mark, uint32_t duration) {
    if (timings.empty() && !mark) return true; // Leading idle is not part of a frame.
    const bool merge = !timings.empty() && (timings.size() % 2 == 1) == mark;
    const uint32_t previous = merge ? timings.back() : 0;
    if (!duration || duration > UINT16_MAX - previous) {
        timings.clear();
        return false;
    }
    if (merge) timings.back() += duration;
    else timings.push_back(static_cast<uint16_t>(duration));
    return true;
}


// Flipper field layouts/timings: flipperdevices/flipperzero-firmware,
// lib/infrared/encoder_decoder/{nec,samsung,sirc,rc5,rc6}.
InfraredWaveform InfraredRemoteTransformer::fromFileCommand(const InfraredFileRemoteCommand& command, bool toggle) {
    InfraredWaveform wave;
    if (!command.error.empty()) return wave;
    auto& pulses = wave.pulses;
    const std::string& protocol = command.fileProtocol;
    if (protocol != "raw" && !isValidParsedCommand(protocol, command.address, command.function)) return wave;
    const uint64_t address = command.address, function = command.function;
    if (protocol == "raw") {
        if (command.frequency < 10000 || command.frequency > 1000000 ||
            !std::isfinite(command.dutyCycle) || command.dutyCycle <= 0 || command.dutyCycle > 1)
            return wave;
        pulses = command.rawData;
        wave.frequencyHz = command.frequency;
        wave.dutyPercent = std::clamp(static_cast<unsigned>(std::lround(command.dutyCycle * 100)), 1u, 100u);
    } else if (protocol == "NEC" || protocol == "NECext" || protocol == "NEC42" ||
               protocol == "NEC42ext" || protocol == "Samsung32") {
        const bool samsung = protocol == "Samsung32";
        const uint32_t mark = samsung ? 550 : 560;
        uint64_t bits = 0;
        unsigned count = 32;
        if (protocol == "NEC" || samsung)
            bits = address | ((samsung ? address : (~address & 255)) << 8) |
                   (function << 16) | ((~function & 255) << 24);
        else if (protocol == "NECext") bits = address | (function << 16);
        else if (protocol == "NEC42") {
            count = 42;
            bits = address | ((~address & 8191) << 13) | (function << 26) | ((~function & 255) << 34);
        } else { count = 42; bits = address | (function << 26); }
        pulses = {samsung ? 4500u : 9000u, 4500};
        for (unsigned bit = 0; bit < count; ++bit) {
            pulses.push_back(mark);
            pulses.push_back((bits >> bit) & 1 ? (samsung ? 1650 : 1690) : mark);
        }
        pulses.push_back(mark);
    } else if (protocol == "SIRC" || protocol == "SIRC15" || protocol == "SIRC20") {
        wave.frequencyHz = 40000;
        const unsigned count = protocol == "SIRC" ? 12 : (protocol == "SIRC15" ? 15 : 20);
        const uint64_t bits = function | (address << 7);
        std::vector<uint32_t> frame {2400, 600};
        for (unsigned bit = 0; bit < count; ++bit) {
            frame.push_back((bits >> bit) & 1 ? 1200 : 600);
            if (bit + 1 < count) frame.push_back(600);
        }
        const uint32_t gap = 45000 - std::accumulate(frame.begin(), frame.end(), uint32_t(0));
        for (int repeat = 0; repeat < 3; ++repeat) {
            if (repeat) pulses.push_back(gap);
            pulses.insert(pulses.end(), frame.begin(), frame.end());
        }
    } else if (protocol == "RC5" || protocol == "RC5X" || protocol == "RC6") {
        wave.frequencyHz = 36000;
        const bool rc6 = protocol == "RC6";
        const uint32_t unit = rc6 ? 444 : 888;
        auto level = [&](bool mark, uint32_t duration) {
            if (pulses.empty() && !mark) return;
            if (!pulses.empty() && (pulses.size() % 2 == 1) == mark) pulses.back() += duration;
            else pulses.push_back(duration);
        };
        auto bit = [&](bool value, unsigned length = 1) {
            level(rc6 ? value : !value, unit * length);
            level(rc6 ? !value : value, unit * length);
        };
        if (rc6) {
            level(true, 2666); level(false, 889);
            bit(true); bit(false); bit(false); bit(false); bit(toggle, 2);
        } else {
            // Flipper stores the extension in the protocol name; its decoder
            // can write RC5X commands with only the low six command bits set.
            bit(true); bit(protocol == "RC5"); bit(toggle);
        }
        for (int i = rc6 ? 7 : 4; i >= 0; --i) bit((address >> i) & 1);
        for (int i = rc6 ? 7 : 5; i >= 0; --i) bit((function >> i) & 1);
    }
    // Keep the complete raw sequence, including its final silence.
    if (protocol != "raw" && !pulses.empty() && pulses.size() % 2 == 0) pulses.pop_back();
    return wave;
}

bool InfraredRemoteTransformer::isValidParsedCommand(const std::string& protocol,
                                                       uint32_t address, uint32_t function) {
    struct Limits { const char* name; uint32_t address; uint32_t function; };
    static constexpr Limits limits[] = {
        {"NEC", 255, 255}, {"NECext", 65535, 65535}, {"Samsung32", 255, 255},
        {"NEC42", 8191, 255}, {"NEC42ext", 0x3ffffff, 65535},
        {"SIRC", 31, 127}, {"SIRC15", 255, 127}, {"SIRC20", 8191, 127},
        {"RC5", 31, 63}, {"RC5X", 31, 127}, {"RC6", 255, 255}
    };
    for (const auto& limit : limits) {
        if (protocol == limit.name) return address <= limit.address && function <= limit.function;
    }
    return false;
}

// Apple/NEC decode normalization shared by both IR receiver backends.
// Conversion of Apple/NEC wire payloads shared by both IR decoder backends.
// The MakeHex Apple encoder sends the payload LSB-first, including 0x87 in
// bits 8..15 and a one-bit parity check in bit 16.


uint32_t InfraredRemoteTransformer::reverseBits32(uint32_t value) {
    value = ((value & 0x55555555u) << 1) | ((value >> 1) & 0x55555555u);
    value = ((value & 0x33333333u) << 2) | ((value >> 2) & 0x33333333u);
    value = ((value & 0x0F0F0F0Fu) << 4) | ((value >> 4) & 0x0F0F0F0Fu);
    value = ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu);
    return (value << 16) | (value >> 16);
}

bool InfraredRemoteTransformer::decodeApplePayload(uint32_t lsbPayload, InfraredCommand& command) {
    const uint8_t device = lsbPayload & 0xFFu;
    if (((lsbPayload >> 8) & 0xFFu) != 0x87u) return false;
    const uint8_t check = (lsbPayload >> 16) & 1u;
    const uint8_t function = (lsbPayload >> 17) & 0x7Fu;
    const uint8_t pairId = (lsbPayload >> 24) & 0xFFu;
    auto parity = [](uint32_t n) {
        uint8_t p = 0;
        for (; n; n >>= 1) p ^= n & 1u;
        return p;
    };
    const uint8_t expected = 1u ^ parity(device) ^ parity(0x87u) ^
                             parity(function) ^ parity(pairId);
    if (check != expected) return false;
    command.setProtocol(InfraredProtocolEnum::_APPLE);
    command.setDevice(device);
    command.setSubdevice(pairId);
    command.setFunction(function);
    return true;
}

bool InfraredRemoteTransformer::decodeAppleEitherBitOrder(uint32_t raw, InfraredCommand& command) {
    return decodeApplePayload(raw, command) ||
           decodeApplePayload(reverseBits32(raw), command);
}
