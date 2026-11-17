#pragma once

#include "Models/InfraredCommand.h"

// Direct IR library send paths pack some fields into 8 bits. Reject values
// that would be silently truncated. The MakeHex path retains its own IRP
// width rules (e.g. NEC42ext legitimately supports wider values).
namespace InfraredCommandLimits {
inline bool valid(const InfraredCommand& cmd) {
    if (cmd.getDevice() < 0 || cmd.getSubdevice() < -1 || cmd.getFunction() < 0)
        return false;
    switch (cmd.getProtocol()) {
        case _NEC:
        case _NEC2:
        case _SAMSUNG:
        case _LG:
        case _SANYO_LC7461:
        case _PANASONIC:
        case PANASONIC_:
        case _PIONEER:
        case LEGO:
            return cmd.getDevice() <= 255 && cmd.getSubdevice() <= 255 &&
                   cmd.getFunction() <= 255;
        default:
            return true;
    }
}
}
