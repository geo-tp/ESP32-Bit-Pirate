#include "Services/MeshtasticService.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

namespace {
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
           (uint32_t(p[3]) << 24);
}
int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
}

const MeshtasticService::Preset* MeshtasticService::presets(size_t& count) {
    static const Preset values[] = {
        {"LONG_FAST", 250, 11, 5}, {"LONG_SLOW", 125, 12, 8},
        {"LONG_MODERATE", 125, 11, 8},
        {"VERY_LONG_SLOW", 62, 12, 8}, {"MEDIUM_SLOW", 250, 10, 5},
        {"MEDIUM_FAST", 250, 9, 5}, {"SHORT_SLOW", 250, 8, 5},
        {"SHORT_FAST", 250, 7, 5}, {"SHORT_TURBO", 500, 7, 5},
    };
    count = sizeof(values) / sizeof(values[0]);
    return values;
}

bool MeshtasticService::profileFor(const std::string& name, float frequency,
                                   LoRaRadioProfile& out) {
    size_t count;
    const Preset* values = presets(count);
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    for (size_t i = 0; i < count; ++i) {
        if (upper != values[i].name) continue;
        out.frequency = frequency;
        out.bandwidth = values[i].bandwidth;
        out.spreadingFactor = values[i].sf;
        out.codingRate = values[i].cr;
        out.power = 14;
        // Meshtastic standard profile: fixed 16-symbol preamble.
        out.preambleLength = 16;
        // Meshtastic uses the logical LoRa sync word 0x2B. SX126x radios
        // expose the expanded 16-bit register representation, 0x24B4.
        out.syncWord = 0x24B4;
        out.crc = true;
        out.invertIq = false;
        out.tcxoVoltage = 1.8f;
        return true;
    }
    return false;
}

bool MeshtasticService::setKey(const std::string& value) {
    static const uint8_t defaultPsk[] = {
        0xD4, 0xF1, 0xBB, 0x3A, 0x20, 0x29, 0x07, 0x59,
        0xF0, 0xBC, 0xFF, 0xAB, 0xCF, 0x4E, 0x69, 0x01,
    };

    std::string compact;
    compact.reserve(value.size());
    for (char c : value) {
        if (!std::isspace(static_cast<unsigned char>(c))) compact += c;
    }
    if (compact.empty()) return false;

    std::string lower = compact;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    bool forceHex = false;
    bool forceBase64 = false;
    if (lower.rfind("hex:", 0) == 0) {
        compact.erase(0, 4);
        forceHex = true;
    } else if (lower.rfind("base64:", 0) == 0) {
        compact.erase(0, 7);
        forceBase64 = true;
    }

    std::string hex;
    hex.reserve(compact.size());
    for (char c : compact) {
        if (c != ':' && c != '-') hex += c;
    }

    bool looksHex = !hex.empty();
    for (char c : hex) {
        if (hexNibble(c) < 0) {
            looksHex = false;
            break;
        }
    }

    if (!forceBase64 && (forceHex || looksHex)) {
        if (hex.size() != 32 && hex.size() != 64) return false;
        std::vector<uint8_t> parsed(hex.size() / 2);
        for (size_t i = 0; i < parsed.size(); ++i) {
            const int hi = hexNibble(hex[i * 2]);
            const int lo = hexNibble(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0) return false;
            parsed[i] = static_cast<uint8_t>((hi << 4) | lo);
        }
        key_ = parsed;
        return true;
    }

    const size_t decodedCapacity =
        ((compact.size() + 3) / 4) * 3;
    if (decodedCapacity == 0) return false;

    std::vector<uint8_t> decoded(decodedCapacity);
    size_t outputLength = 0;
    const int decodeResult = mbedtls_base64_decode(
        decoded.data(), decoded.size(), &outputLength,
        reinterpret_cast<const unsigned char*>(compact.data()),
        compact.size());
    if (decodeResult != 0) return false;
    decoded.resize(outputLength);

    if (decoded.size() == 1 && decoded[0] == 0x01) {
        key_.assign(defaultPsk, defaultPsk + sizeof(defaultPsk));
        return true;
    }
    if (decoded.size() != 16 && decoded.size() != 32) return false;

    key_ = decoded;
    return true;
}
void MeshtasticService::clearKey() { key_.clear(); }

void MeshtasticService::appendLe32(std::vector<uint8_t>& bytes,
                                   uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 24));
}

void MeshtasticService::appendVarint(std::vector<uint8_t>& bytes,
                                     uint64_t value) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7Fu);
        value >>= 7;
        if (value != 0) byte |= 0x80u;
        bytes.push_back(byte);
    } while (value != 0);
}

uint8_t MeshtasticService::channelHash(
        const std::string& channelName) const {
    uint8_t hash = 0;
    for (const char c : channelName) {
        hash ^= static_cast<uint8_t>(c);
    }
    for (const uint8_t byte : key_) hash ^= byte;
    return hash;
}

bool MeshtasticService::readVarint(const uint8_t*& p, const uint8_t* end, uint64_t& v) {
    v = 0;
    for (unsigned shift = 0; p < end && shift < 64; shift += 7) {
        uint8_t b = *p++; v |= uint64_t(b & 0x7f) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}
bool MeshtasticService::skipField(uint8_t wire, const uint8_t*& p, const uint8_t* end) {
    uint64_t n;
    if (wire == 0) return readVarint(p, end, n);
    if (wire == 1 && end - p >= 8) { p += 8; return true; }
    if (wire == 5 && end - p >= 4) { p += 4; return true; }
    if (wire == 2 && readVarint(p, end, n) && n <= uint64_t(end - p)) { p += n; return true; }
    return false;
}
std::string MeshtasticService::portName(uint32_t p) {
    switch (p) {
        case 1: return "TEXT_MESSAGE_APP"; case 3: return "POSITION_APP";
        case 4: return "NODEINFO_APP"; case 67: return "TELEMETRY_APP";
        case 70: return "TRACEROUTE_APP"; case 71: return "NEIGHBORINFO_APP";
        default: return "PORT_" + std::to_string(p);
    }
}
bool MeshtasticService::decodeData(const std::vector<uint8_t>& b, MeshtasticPacket& packet) const {
    const uint8_t* p = b.data(); const uint8_t* end = p + b.size();
    while (p < end) {
        uint64_t tag;
        if (!readVarint(p, end, tag)) return false;
        uint32_t field = tag >> 3; uint8_t wire = tag & 7;
        if (field == 1 && wire == 0) {
            uint64_t port; if (!readVarint(p, end, port)) return false;
            packet.portNum = static_cast<uint32_t>(port); packet.type = portName(packet.portNum);
        } else if (field == 2 && wire == 2) {
            uint64_t len; if (!readVarint(p, end, len) || len > uint64_t(end - p)) return false;
            packet.payload.assign(p, p + len); p += len;
        } else if (!skipField(wire, p, end)) return false;
    }
    packet.encrypted = false;
    if (packet.portNum == 1) packet.text.assign(packet.payload.begin(), packet.payload.end());
    return packet.portNum != 0;
}


bool MeshtasticService::cryptPayload(
        uint32_t source, uint32_t packetId,
        const std::vector<uint8_t>& input,
        std::vector<uint8_t>& output) const {
    output.clear();
    if (key_.empty() || input.empty()) return false;

    uint8_t nonce[16]{};
    nonce[0] = static_cast<uint8_t>(packetId);
    nonce[1] = static_cast<uint8_t>(packetId >> 8);
    nonce[2] = static_cast<uint8_t>(packetId >> 16);
    nonce[3] = static_cast<uint8_t>(packetId >> 24);
    nonce[8] = static_cast<uint8_t>(source);
    nonce[9] = static_cast<uint8_t>(source >> 8);
    nonce[10] = static_cast<uint8_t>(source >> 16);
    nonce[11] = static_cast<uint8_t>(source >> 24);

    uint8_t streamBlock[16]{};
    size_t offset = 0;
    output.resize(input.size());

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_enc(
        &aes, key_.data(), static_cast<unsigned int>(key_.size() * 8));
    if (keyResult != 0) {
        mbedtls_aes_free(&aes);
        output.clear();
        return false;
    }

    const int cryptResult = mbedtls_aes_crypt_ctr(
        &aes, input.size(), &offset, nonce, streamBlock,
        input.data(), output.data());
    mbedtls_aes_free(&aes);

    if (cryptResult != 0) {
        output.clear();
        return false;
    }
    return true;
}

bool MeshtasticService::decryptPayload(
        const MeshtasticPacket& packet,
        std::vector<uint8_t>& output) const {
    return cryptPayload(packet.source, packet.id,
                        packet.payload, output);
}

bool MeshtasticService::buildTextFrame(
        const std::string& text,
        const std::string& channelName,
        uint32_t source,
        uint32_t packetId,
        std::vector<uint8_t>& frame,
        std::string& error) const {
    frame.clear();
    error.clear();

    if (text.empty()) {
        error = "text is empty";
        return false;
    }
    if (source == 0 || source == 0xFFFFFFFFu || packetId == 0) {
        error = "invalid node or packet ID";
        return false;
    }

    std::vector<uint8_t> data;
    data.reserve(text.size() + 8);
    data.push_back(0x08);  // Data.portnum
    data.push_back(0x01);  // TEXT_MESSAGE_APP
    data.push_back(0x12);  // Data.payload
    appendVarint(data, text.size());
    data.insert(data.end(), text.begin(), text.end());

    std::vector<uint8_t> payload;
    if (hasKey()) {
        if (!cryptPayload(source, packetId, data, payload)) {
            error = "payload encryption failed";
            return false;
        }
    } else {
        payload = data;
    }

    constexpr size_t kHeaderSize = 16;
    constexpr size_t kMaxFrameSize = 255;
    if (kHeaderSize + payload.size() > kMaxFrameSize) {
        error = "text is too long";
        return false;
    }

    frame.reserve(kHeaderSize + payload.size());
    appendLe32(frame, 0xFFFFFFFFu);  // broadcast
    appendLe32(frame, source);
    appendLe32(frame, packetId);

    constexpr uint8_t kHopLimit = 3;
    const uint8_t flags = static_cast<uint8_t>(
        kHopLimit | (kHopLimit << 5));
    frame.push_back(flags);
    frame.push_back(channelHash(channelName));
    frame.push_back(0);  // no next-hop preference
    frame.push_back(0);  // no relay node
    frame.insert(frame.end(), payload.begin(), payload.end());
    return true;
}

bool MeshtasticService::inspect(const std::vector<uint8_t>& frame, MeshtasticPacket& packet,
                                std::string& error) const {
    if (frame.size() < 16) { error = "frame shorter than 16-byte radio header"; return false; }
    packet = MeshtasticPacket{};
    packet.destination = le32(&frame[0]); packet.source = le32(&frame[4]); packet.id = le32(&frame[8]);
    packet.hopLimit = frame[12] & 0x07; packet.wantAck = (frame[12] & 0x08) != 0;
    packet.viaMqtt = (frame[12] & 0x10) != 0; packet.hopStart = (frame[12] >> 5) & 0x07;
    packet.channel = frame[13]; packet.payload.assign(frame.begin() + 16, frame.end());
    // Unencrypted packets contain a protobuf Data message directly. Channel
    // packets can also be decoded with a configured AES CTR PSK. Packets that
    // use another channel key or Meshtastic PKC remain inspectable at the
    // clear network header and raw payload levels.
    MeshtasticPacket decoded = packet;
    if (decodeData(packet.payload, decoded)) {
        packet = decoded;
        error.clear();
        return true;
    }

    std::vector<uint8_t> decrypted;
    decoded = packet;
    if (decryptPayload(packet, decrypted) && decodeData(decrypted, decoded)) {
        packet = decoded;
    }

    error.clear();
    return true;
}
