#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "Enums/InfraredProtocolEnum.h"

struct InfraredFileRemoteCommand {
    std::string functionName;
    InfraredProtocolEnum protocol = _IR_UNKNOWN;
    uint32_t address = 0;
    uint32_t function = 0;
    std::vector<uint32_t> rawData;
    uint32_t frequency = 38000; // Hz, including import/export and encoding.
    float dutyCycle = 0.33f;
    // Flipper names and field layouts are distinct from MakeHex D/S/F.
    std::string fileProtocol;
    std::string error;
    bool legacyDsf = false; // Older Bit Pirate files used MakeHex names and packed D/S.
};

inline bool operator==(const InfraredFileRemoteCommand& a, const InfraredFileRemoteCommand& b) {
    return a.functionName == b.functionName && a.protocol == b.protocol &&
           a.address == b.address && a.function == b.function && a.rawData == b.rawData &&
           a.frequency == b.frequency && a.dutyCycle == b.dutyCycle &&
           a.fileProtocol == b.fileProtocol && a.error == b.error && a.legacyDsf == b.legacyDsf;
}
inline bool operator!=(const InfraredFileRemoteCommand& a, const InfraredFileRemoteCommand& b) {
    return !(a == b);
}
