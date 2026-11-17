#pragma once

#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <Enums/InfraredProtocolEnum.h>

class InfraredCommand {
public:
    InfraredCommand()
        : functionName(""), protocol(InfraredProtocolEnum::_RAW),
          device(0), subdevice(0), function(0) {}

    InfraredCommand(InfraredProtocolEnum proto, int device, int subdevice, int function)
        : functionName(""), protocol(proto), device(device), subdevice(subdevice), function(function) {}


    InfraredCommand(const char* functionName, InfraredProtocolEnum protocol, int16_t device, int16_t subdevice, int16_t function)
        : functionName(functionName), protocol(protocol), device(device), subdevice(subdevice), function(function) {}

    const char* getFunctionName() const { return functionName; }
    InfraredProtocolEnum getProtocol() const { return protocol; }
    int16_t getDevice() const { return device; }
    int16_t getSubdevice() const { return subdevice; }
    int16_t getFunction() const { return function; }

    void setDevice(int16_t value) { device = value; }
    void setSubdevice(int16_t value) { subdevice = value; }
    void setFunction(int16_t value) { function = value; }
    void setProtocol(InfraredProtocolEnum prot)  {protocol = prot; }

    // Optional reception metadata. A library may decode a protocol with fields
    // wider than Bit Pirate's legacy 16-bit device/function representation.
    // Keep the native identification instead of truncating or dropping it.
    void setNativeDecode(const std::string& name, uint16_t bits, uint64_t value,
                         bool repeat = false) {
        decodedProtocolName = name;
        decodedBits = bits;
        decodedRawData = value;
        decodedRepeat = repeat;
        decodedState.clear();
        decodedStateTruncated = false;
    }
    const std::string& getDecodedProtocolName() const { return decodedProtocolName; }
    std::string getDisplayProtocolName() const {
        return protocol == InfraredProtocolEnum::_IR_UNKNOWN && !decodedProtocolName.empty()
            ? decodedProtocolName : InfraredProtocolMapper::toString(protocol);
    }
    bool hasNativeDecode() const { return !decodedProtocolName.empty(); }
    uint16_t getDecodedBits() const { return decodedBits; }
    uint64_t getDecodedRawData() const { return decodedRawData; }
    bool isDecodedRepeat() const { return decodedRepeat; }
    // Avoid heap allocations for ordinary short commands. Long A/C states
    // are copied only on demand, and at most 96 bytes are retained.
    static constexpr size_t MAX_DECODED_STATE_BYTES = 96;
    void setDecodedState(const uint8_t* data, size_t count) {
        decodedState.clear();
        if (data && count) {
            const size_t length = std::min(count, MAX_DECODED_STATE_BYTES);
            decodedState.assign(data, data + length);
        }
        decodedStateTruncated = count > MAX_DECODED_STATE_BYTES;
    }
    const std::vector<uint8_t>& getDecodedState() const { return decodedState; }
    bool isDecodedStateTruncated() const { return decodedStateTruncated; }
    void markDecodedStateTruncated() { decodedStateTruncated = true; }

    bool operator==(const InfraredCommand& other) const {
        return (std::strcmp(functionName, other.functionName) == 0) &&
               (protocol == other.protocol) &&
               (device == other.device) &&
               (subdevice == other.subdevice) &&
               (function == other.function);
    }

    bool operator!=(const InfraredCommand& other) const {
        return !(*this == other);
    }

private:
    const char* functionName;
    InfraredProtocolEnum protocol;
    int16_t device;
    int16_t subdevice;
    int16_t function;
    std::string decodedProtocolName;
    uint64_t decodedRawData = 0;
    uint16_t decodedBits = 0;
    bool decodedRepeat = false;
    bool decodedStateTruncated = false;
    std::vector<uint8_t> decodedState;
};
