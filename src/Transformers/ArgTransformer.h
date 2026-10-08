#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <algorithm>

class ArgTransformer {
public:
    std::vector<uint8_t> parseByteList(const std::string& input) const;
    std::vector<uint8_t> parseHexList(const std::string& input) const;
    std::vector<uint16_t> parseHexList16(const std::string& input) const;
    uint8_t parseByte(const std::string& str, int index = -1) const;
    uint8_t parseHexOrDec(const std::string& s) const ;
    uint16_t parseHexOrDec16(const std::string& str) const;
    uint32_t parseHexOrDec32(const std::string& s) const ;
    uint64_t parseHexOrDec64(const std::string& s) const;
    bool parseHexBytes(const std::string& s, uint8_t* out, uint8_t expectedLen);
    bool parseInt(const std::string& input, int& output);
    std::vector<std::string> splitArgs(const std::string& input);
    bool isValidNumber(const std::string& input);
    bool isValidFloat(const std::string& input);
    bool isValidNumericCode(const std::string& input, size_t minLen, size_t maxLen);
    bool isValidAlphanumeric(const std::string& input);
    uint8_t toUint8(const std::string& input);
    uint32_t toUint32(const std::string& input);
    std::string toLower(const std::string& input);
    std::string filterPrintable(const std::string& input);
    std::string decodeEscapes(const std::string& input);
    int8_t toClampedInt8(const std::string& input);
    bool isValidSignedNumber(const std::string& input);
    std::string toHex(uint32_t value, int width = 2);
    std::string formatFloat(double value, int decimals);
    std::string toAsciiLine(uint32_t address, const std::vector<uint8_t>& line);
    std::string toAsciiLine(uint32_t startAddr, const std::vector<uint16_t>& words);
    std::string formatHexAscii(const uint8_t* data, size_t len, bool withAscii = true, size_t bytesPerRow = 16);
    std::string toBinString(uint32_t value);
    std::string toAsciiString(uint32_t value);
    bool parseMac(const std::string& s, std::array<uint8_t,6>& out);
    std::string ensureHttpScheme(std::string u);
    std::string normalizeLines(const std::string& in);
    std::string extractHostFromUrl(const std::string& url);
    std::vector<uint8_t> parse01List(const std::string& line);
    std::vector<uint8_t> packLsbFirst(const std::vector<uint8_t>& bits);
    bool unpackLsbFirst(const std::vector<uint8_t>& bytes,
                        size_t qty,
                        std::vector<uint8_t>& outBits);
    std::string toFixed2(float f);
    bool parsePattern(const std::string& patternRaw,
                                std::string& outTextPattern,
                                std::vector<uint8_t>& outHexPattern,
                                std::vector<uint8_t>& outHexMask,
                                bool& outIsHex) ;
};
