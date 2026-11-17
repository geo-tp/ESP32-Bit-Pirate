#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <Models/InfraredFileRemoteCommand.h>
#include <Enums/InfraredProtocolEnum.h>
#include <Models/InfraredWaveform.h>
#include <Models/InfraredCommand.h>

class InfraredRemoteTransformer {
public:
    static constexpr size_t MAX_FILE_SIZE = 32 * 1024;
    // A single command is loaded at a time; keep its temporary textual payload bounded.
    static constexpr size_t MAX_COMMAND_SIZE = 8 * 1024;
    static constexpr size_t MAX_COMMAND_COUNT = 64;
    // Flipper .ir import, export and bounded recording.
    static bool isValidInfraredFile(const std::string& fileContent);
    static std::vector<InfraredFileRemoteCommand> transformFromFileFormat(const std::string& fileContent);
    static std::string transformToFileFormat(const std::string& fileName, const std::vector<InfraredFileRemoteCommand>& cmds);
    // Append a captured frame only if the complete file remains reloadable.
    static bool appendRawCommand(std::string& file, const std::string& name,
                                 const std::vector<uint16_t>& timings, uint32_t khz,
                                 size_t maxBytes = MAX_FILE_SIZE);
    static std::vector<std::string> extractFunctionNames(const std::vector<InfraredFileRemoteCommand>& cmds);

    // Convert parsed/RAW .ir commands to precise transmission timings.
    static InfraredWaveform fromFileCommand(const InfraredFileRemoteCommand& command, bool toggle);
    static bool isValidParsedCommand(const std::string& protocol, uint32_t address, uint32_t function);
    // Capture remains 16-bit: reject durations that cannot be represented.
    static bool appendRawTiming(std::vector<uint16_t>& timings, bool mark, uint32_t duration);

    // Normalize decoded Apple/NEC wire payloads from either IR backend.
    static uint32_t reverseBits32(uint32_t value);
    static bool decodeApplePayload(uint32_t lsbPayload, InfraredCommand& command);
    static bool decodeAppleEitherBitOrder(uint32_t raw, InfraredCommand& command);
private:
    static std::string toHexBytesLE(uint32_t value, size_t byteCount);
};
