#pragma once

#include <stdint.h>
#include <vector>
#include <string>

#include "Models/InfraredCommand.h"
#include "Enums/InfraredProtocolEnum.h"
#include "Interfaces/IInfraredService.h"
#include "Models/InfraredFileRemoteCommand.h"

/*
There are two backends for infrared:
- IRremoteESP8266: supports more protocols. Enabled with INFRARED_IREMOTE_ESP8266 define.
- Arduino-IRremote: by default, works on all platforms but supports fewer protocols.
- MakeHex is used for unsupported protocols on both backends
*/

#ifdef INFRARED_IREMOTE_ESP8266

#include <IRsend.h>
#include <IRrecv.h>

class InfraredService : public IInfraredService {
public:
    InfraredService() = default;
    ~InfraredService();

    void configure(uint8_t tx, uint8_t rx);

    void startReceiver();
    void stopReceiver();

    void sendInfraredCommand(InfraredCommand command);
    void sendInfraredFileCommand(InfraredFileRemoteCommand command);
    InfraredCommand receiveInfraredCommand();
    bool receiveRaw(std::vector<uint16_t>& timings, uint32_t& khzAllowGuess);
    void sendRaw(const std::vector<uint16_t>& timings, uint32_t khz);
    void sendJam(uint8_t modeIndex,
                 uint16_t khz,
                 uint32_t& sweepIndex,
                 uint8_t density);

    std::vector<std::string> getCarrierStrings();
    std::vector<std::string> getJamModeStrings();

private:
    enum class JamMode : uint8_t { CARRIER = 0, SWEEP = 1, RANDOM = 2 };
    inline static const uint16_t carrierKhz[] = {36, 38, 40, 56, 57, 58};
    inline static constexpr uint16_t kRawBufferSize = 512; //Increase raw buffer size from its default value of 100
    inline static constexpr uint32_t IR_DEFAULT_FREQUENCY_KHZ = 38;

    // Helpers
    uint16_t getKaseikyoVendorIdCode(const std::string& input);
    static uint16_t packAddress(uint8_t device, int subdevice);
    
    bool _configured = false;
    uint8_t _txPin = 0xFF;
    uint8_t _rxPin = 0xFF;
    IRsend* _sender = nullptr;
    IRrecv* _receiver = nullptr;
    decode_results _results;
};

#else

class InfraredService : public IInfraredService {
public:
    enum class JamMode : uint8_t { CARRIER, SWEEP, RANDOM };
    void configure(uint8_t tx, uint8_t rx);
    void startReceiver();
    void stopReceiver();
    void sendInfraredCommand(InfraredCommand command);
    void sendInfraredFileCommand(InfraredFileRemoteCommand command);
    InfraredCommand receiveInfraredCommand();
    bool receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz);
    void sendRaw(const std::vector<uint16_t>& timings, uint32_t khz);
    void sendJam(uint8_t modeIndex,
        uint16_t khz,
        uint32_t& sweepIndex,
        uint8_t density);
    std::vector<std::string> getCarrierStrings();
    std::vector<std::string> getJamModeStrings();

protected:
    void configureTransmitter(uint8_t tx);

    inline static constexpr uint32_t IR_DEFAULT_FREQUENCY_KHZ = 38;

private:
    inline static constexpr uint16_t carrierKhz[] = {
        30, 33, 36, 38, 40, 42, 56
    };

    uint16_t getKaseikyoVendorIdCode(const std::string& input);
};

#endif
