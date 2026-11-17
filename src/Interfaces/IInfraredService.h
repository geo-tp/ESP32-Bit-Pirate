#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Models/InfraredCommand.h"
#include "Models/InfraredFileRemoteCommand.h"

class IInfraredService {
public:
    virtual ~IInfraredService() = default;

    virtual void configure(uint8_t tx, uint8_t rx) = 0;
    virtual void startReceiver() = 0;
    virtual void stopReceiver() = 0;
    virtual bool sendInfraredCommand(InfraredCommand command) = 0;
    virtual bool sendInfraredFileCommand(const InfraredFileRemoteCommand& command) = 0;
    virtual InfraredCommand receiveInfraredCommand() = 0;
    virtual bool receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz) = 0;
    virtual void sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) = 0;
    virtual void sendJam(uint8_t modeIndex,
                         uint16_t khz,
                         uint32_t& sweepIndex,
                         uint8_t density) = 0;
    virtual std::vector<std::string> getCarrierStrings() = 0;
    virtual std::vector<std::string> getJamModeStrings() = 0;
};
