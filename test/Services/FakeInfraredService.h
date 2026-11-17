#pragma once
#include <functional>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "Interfaces/IInfraredService.h"

class FakeInfraredService final : public IInfraredService {
public:
    struct Configuration {
        uint8_t tx = 0;
        uint8_t rx = 0;
    };

    std::vector<Configuration> configurations;
    std::vector<InfraredCommand> sentCommands;
    std::vector<InfraredFileRemoteCommand> sentFileCommands;
    std::vector<std::vector<uint16_t>> sentRawTimings;
    std::vector<uint32_t> sentRawKhz;
    std::deque<InfraredCommand> decodedQueue;

    struct RawFrame {
        std::vector<uint16_t> timings;
        uint32_t khz = 0;
    };
    std::deque<RawFrame> rawQueue;

    std::vector<std::string> carrierStrings{"36", "38", "40"};
    std::vector<std::string> jamModeStrings{"random", "carrier"};

    bool sendResult = true;
    std::function<void()> onReceiveRaw;
    std::function<void(const std::vector<uint16_t>&)> onSendRaw;
    uint32_t startReceiverCalls = 0;
    uint32_t stopReceiverCalls = 0;
    uint32_t sendJamCalls = 0;
    uint8_t lastJamMode = 0;
    uint16_t lastJamKhz = 0;
    uint8_t lastJamDensity = 0;

    void configure(uint8_t tx, uint8_t rx) override {
        configurations.push_back({tx, rx});
    }

    void startReceiver() override { ++startReceiverCalls; }
    void stopReceiver() override { ++stopReceiverCalls; }

    bool sendInfraredCommand(InfraredCommand command) override {
        sentCommands.push_back(command);
        return sendResult;
    }

    bool sendInfraredFileCommand(const InfraredFileRemoteCommand& command) override {
        sentFileCommands.push_back(command);
        return sendResult;
    }

    InfraredCommand receiveInfraredCommand() override {
        if (decodedQueue.empty()) return InfraredCommand();
        InfraredCommand command = decodedQueue.front();
        decodedQueue.pop_front();
        return command;
    }

    bool receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz) override {
        if (rawQueue.empty()) return false;
        RawFrame frame = rawQueue.front();
        rawQueue.pop_front();
        timings = frame.timings;
        khz = frame.khz;
        if (onReceiveRaw) onReceiveRaw();
        return true;
    }

    void sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) override {
        sentRawTimings.push_back(timings);
        sentRawKhz.push_back(khz);
        if (onSendRaw) onSendRaw(timings);
    }

    void sendJam(uint8_t modeIndex,
                 uint16_t khz,
                 uint32_t& sweepIndex,
                 uint8_t density) override {
        ++sendJamCalls;
        lastJamMode = modeIndex;
        lastJamKhz = khz;
        lastJamDensity = density;
        ++sweepIndex;
    }

    std::vector<std::string> getCarrierStrings() override { return carrierStrings; }
    std::vector<std::string> getJamModeStrings() override { return jamModeStrings; }

    void queueDecoded(InfraredProtocolEnum protocol, int device, int subdevice, int function) {
        decodedQueue.emplace_back(protocol, device, subdevice, function);
    }

    void queueRaw(std::vector<uint16_t> timings, uint32_t khz) {
        rawQueue.push_back({std::move(timings), khz});
    }
};
