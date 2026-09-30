#include "ArgTransformer.h"
#include <array>
#include <cerrno>
#include <climits>

uint8_t ArgTransformer::parseByte(const std::string& str, int index) const {
    (void)index;
    try {
        size_t parsedChars = 0;
        const int base = str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0 ? 16 : 10;
        const unsigned long value = std::stoul(str, &parsedChars, base);
        if (parsedChars != str.size() || value > 0xFF) return 0xFF;
        return static_cast<uint8_t>(value);
    } catch (...) {
        return 0xFF;
    }
}

std::vector<uint8_t> ArgTransformer::parseByteList(const std::string& input) const {
    std::istringstream iss(input);
    std::string token;
    std::vector<uint8_t> bytes;

    while (iss >> token) {
        try {
            size_t parsedChars = 0;
            const int base = token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0 ? 16 : 10;
            const unsigned long value = std::stoul(token, &parsedChars, base);
            if (parsedChars == token.size() && value <= 0xFF) {
                bytes.push_back(static_cast<uint8_t>(value));
            }
        } catch (...) {
        }
    }

    return bytes;
}

std::vector<uint8_t> ArgTransformer::parseHexList(const std::string& input) const {
    std::vector<uint8_t> result;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) {
        try {
            size_t parsedChars = 0;
            const unsigned long value = std::stoul(token, &parsedChars, 16);
            if (parsedChars == token.size() && value <= 0xFF) {
                result.push_back(static_cast<uint8_t>(value));
            }
        } catch (...) {
        }
    }

    return result;
}

std::vector<uint16_t> ArgTransformer::parseHexList16(const std::string& input) const {
    std::vector<uint16_t> result;

    std::string normalized = input;
    for (char& c : normalized) {
        if (c == ',' || c == ';') c = ' ';
    }

    std::istringstream iss(normalized);
    std::string token;

    while (iss >> token) {
        try {
            size_t pos = 0;
            unsigned long v = std::stoul(token, &pos, 16);
            if (pos != token.size()) continue;
            if (v <= 0xFFFF) result.push_back(static_cast<uint16_t>(v));
        } catch (...) {
            // token invalide
        }
    }

    return result;
}


uint8_t ArgTransformer::parseHexOrDec(const std::string& str) const {
    if (str.empty()) return 0;

    int base = 10;
    const char* cstr = str.c_str();

    // Check Hex format
    if (str.size() > 2 && (str[0] == '0') && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
        cstr += 2; // prefix 0x
    }

    // Check chars
    for (size_t i = 0; i < strlen(cstr); ++i) {
        char c = cstr[i];
        if (base == 10 && !isdigit(c)) return 0;
        if (base == 16 && !isxdigit(c)) return 0;
    }

    long value = strtol(str.c_str(), nullptr, base);
    if (value < 0 || value > 255) return 0;

    return static_cast<uint8_t>(value);
}

uint16_t ArgTransformer::parseHexOrDec16(const std::string& str) const {
    uint32_t value = parseHexOrDec32(str);
    return static_cast<uint16_t>(value & 0xFFFF);
}

uint32_t ArgTransformer::parseHexOrDec32(const std::string& str) const {
    if (str.empty()) return 0;

    int base = 10;
    const char* cstr = str.c_str();

    // Check Hex format
    if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
        cstr += 2;
    }

    // Check chars
    for (size_t i = 0; i < strlen(cstr); ++i) {
        char c = cstr[i];
        if (base == 10 && !isdigit(c)) return 0;
        if (base == 16 && !isxdigit(c)) return 0;
    }

    if (*cstr == '\0') return 0;

    errno = 0;
    char* end = nullptr;
    unsigned long long value = strtoull(str.c_str(), &end, base);
    if (end == str.c_str() || *end != '\0' || errno == ERANGE || value > 0xFFFFFFFFULL) {
        return 0;
    }
    return static_cast<uint32_t>(value);
}

uint64_t ArgTransformer::parseHexOrDec64(const std::string& str) const {
    if (str.empty()) return 0;

    int base = 10;
    const char* cstr = str.c_str();

    // Hex prefix
    if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
        cstr += 2;
    }

    // Syntax check
    if (*cstr == '\0') return 0;
    for (const char* p = cstr; *p; ++p) {
        char c = *p;
        if (base == 10 && !isdigit((unsigned char)c)) return 0;
        if (base == 16 && !isxdigit((unsigned char)c)) return 0;
    }

    errno = 0;
    char* end = nullptr;
    unsigned long long v = strtoull(str.c_str(), &end, base);

    // end check + overflow check
    if (end == str.c_str() || *end != '\0') return 0;
    if (errno == ERANGE) return 0;

    return (uint64_t)v;
}

bool ArgTransformer::parseHexBytes(const std::string& s, uint8_t* out, uint8_t expectedLen) {
    if (!out || expectedLen == 0) return false;

    std::string hex;
    hex.reserve(expectedLen * 2);

    for (char c : s) {
        if (isxdigit((unsigned char)c)) {
            hex.push_back(c);
        }
    }

    if (hex.size() != expectedLen * 2) return false;

    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    for (int i = 0; i < expectedLen; i++) {
        int hi = nib(hex[i * 2]);
        int lo = nib(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

std::vector<std::string> ArgTransformer::splitArgs(const std::string& input) {
    std::vector<std::string> result;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

bool ArgTransformer::parseInt(const std::string& input, int& output) {
    if (input.empty()) return false;

    std::string s = input;
    int base = 10;

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        base = 16;
    } else if (!s.empty() && (s.back() == 'h' || s.back() == 'H')) {
        base = 16;
        s.pop_back();
        if (s.empty()) return false;
    }

    const char* p = s.c_str();
    char* end = nullptr;

    errno = 0;
    long v = strtol(p, &end, base);
    if (end == p || *end != '\0') return false;
    if (errno == ERANGE) return false;
    if (v < INT_MIN || v > INT_MAX) return false;

    output = (int)v;
    return true;
}

bool ArgTransformer::isValidNumber(const std::string& input) {
    std::string s = input;
    int base = 10;
    if (s.empty()) return false;

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        base = 16;
        s = s.substr(2);
    }

    if (s.empty()) return false;

    for (char c : s) {
        if ((base == 10 && !isdigit(c)) ||
            (base == 16 && !isxdigit(c))) {
            return false;
        }
    }

    return true;
}

bool ArgTransformer::isValidFloat(const std::string& input) {
    std::istringstream iss(input);
    float f;
    iss >> std::noskipws >> f;
    return iss.eof() && !iss.fail();
}

bool ArgTransformer::isValidNumericCode(const std::string& val,
                                          size_t minLen,
                                          size_t maxLen)
{
    if (minLen > maxLen) std::swap(minLen, maxLen);

    if (val.size() < minLen || val.size() > maxLen)
        return false;

    return std::all_of(val.begin(), val.end(),
                       [](char c){ return c >= '0' && c <= '9'; });
}

bool ArgTransformer::isValidSignedNumber(const std::string& input) {
    if (input.empty()) return false;

    std::string s = input;
    int base = 10;
    size_t start = 0;

    // Optional sign
    if (s[0] == '-' || s[0] == '+') {
        if (s.size() == 1) return false;  // - or + alone is not valid
        start = 1;
    }

    // Check for hex prefix
    if (s.find("0x", start) == start || s.find("0X", start) == start) {
        base = 16;
        start += 2;
    }

    if (start >= s.size()) return false;

    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if ((base == 10 && !isdigit(c)) || (base == 16 && !isxdigit(c))) {
            return false;
        }
    }

    return true;
}

bool ArgTransformer::isValidAlphanumeric(const std::string& input) {
    if (input.empty()) return false;

    for (unsigned char c : input) {
        if (!std::isalnum(c)) {
            return false;
        }
    }

    return true;
}

uint8_t ArgTransformer::toUint8(const std::string& input) {
    if (input.empty()) return 0;

    const char* s = input.c_str();
    char* end = nullptr;

    // refuse leading sign for uint8
    if (*s == '-' || *s == '+') return 0;

    int base = 10;
    if (input.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
    }

    errno = 0;
    unsigned long v = strtoul(s, &end, base);
    if (end == s || *end != '\0') return 0;     // non-numeric tail
    if (errno == ERANGE) return 255;              // overflow
    if (v > 255UL) return 255;                    // clamp policy: reject -> 0
    return (uint8_t)v;
}

int8_t ArgTransformer::toClampedInt8(const std::string& input) {
    if (input.empty()) return 0;

    const char* s = input.c_str();
    char* end = nullptr;

    int base = 10;
    size_t start = 0;
    if (s[0] == '+' || s[0] == '-') start = 1;

    if (input.size() > start + 2 && input[start] == '0' && (input[start+1] == 'x' || input[start+1] == 'X')) {
        base = 16;
    }

    errno = 0;
    long v = strtol(s, &end, base);
    if (end == s || *end != '\0') return 0;
    if (errno == ERANGE) {
        // out of range -> saturate
        return (s[0] == '-') ? (int8_t)-127 : (int8_t)127;
    }

    if (v < -127L) v = -127L;
    if (v > 127L)  v = 127L;
    return (int8_t)v;
}

uint32_t ArgTransformer::toUint32(const std::string& input) {
    if (input.empty()) return 0;

    const char* s = input.c_str();
    char* end = nullptr;

    // refuse sign
    if (*s == '-' || *s == '+') return 0;

    int base = 10;
    if (input.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;

    errno = 0;
    unsigned long v = strtoul(s, &end, base);
    if (end == s || *end != '\0') return 0;
    if (errno == ERANGE) return 0;

    if (v > 0xFFFFFFFFUL) return 0;

    return (uint32_t)v;
}

std::string ArgTransformer::toLower(const std::string& input) {
    std::string result = input;
    std::transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); }
    );
    return result;
}

std::string ArgTransformer::filterPrintable(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (std::isprint(static_cast<unsigned char>(c)) || c == '\n' || c == '\r' || c == '\t') {
            result += c;
        }
    }
    return result;
}

std::string ArgTransformer::decodeEscapes(const std::string& input) {
    std::ostringstream out;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            char next = input[++i];
            switch (next) {
                case 'n': out << '\n'; break;
                case 'r': out << '\r'; break;
                case 't': out << '\t'; break;
                case '0': out << '\0'; break;
                case '\\': out << '\\'; break;
                case 'x':
                    if (i + 2 < input.length()) {
                        std::string hex = input.substr(i + 1, 2);
                        try {
                            out << static_cast<char>(std::stoi(hex, nullptr, 16));
                        } catch (...) {
                            out << "\\x" << hex; // Invalid
                        }
                        i += 2;
                    } else {
                        out << "\\x"; // Incomplete
                    }
                    break;
                default:
                    out << '\\' << next; // Unrecognized,
                    break;
            }
        } else {
            out << input[i];
        }
    }
    return out.str();
}

std::string ArgTransformer::toHex(uint32_t value, int width) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return ss.str();
}

std::string ArgTransformer::formatFloat(double value, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), ("%." + std::to_string(decimals) + "f").c_str(), value);
    return std::string(buf);
}

std::string ArgTransformer::toAsciiLine(uint32_t address, const std::vector<uint8_t>& line) {
    std::stringstream ss;

    // Address
    ss << std::hex << std::uppercase << std::setfill('0')
       << std::setw(6) << address << ": ";

    // Hex
    for (size_t i = 0; i < 16; ++i) {
        if (i < line.size()) {
            ss << std::setw(2) << (int)line[i] << " ";
        } else {
            ss << "   ";
        }
    }

    // ASCII
    ss << " ";
    for (size_t i = 0; i < line.size(); ++i) {
        char c = static_cast<char>(line[i]);
        ss << (isprint(c) ? c : '.');
    }

    return ss.str();
}

std::string ArgTransformer::toAsciiLine(uint32_t startAddr, const std::vector<uint16_t>& words) {
    std::stringstream line;

    // Adress
    line << std::hex << std::uppercase << std::setfill('0')
         << std::setw(6) << startAddr << ": ";

    // HEX : 8 x 16-bit (4 hex digits)
    for (size_t i = 0; i < words.size(); ++i) {
        line << std::setw(4) << words[i] << " ";
    }

    // Padding
    for (size_t i = words.size(); i < 8; ++i) {
        line << "     ";
    }

    // ASCII
    line << " ";
    for (uint16_t word : words) {
        char high = (word >> 8) & 0xFF;
        char low  = word & 0xFF;

        line << (isprint(high) ? high : '.');
        line << (isprint(low)  ? low  : '.');
    }

    return line.str();
}

std::string ArgTransformer::formatHexAscii(const uint8_t* data, size_t len, bool withAscii, size_t bytesPerRow) {
    std::stringstream out;

    if (!data || len == 0 || bytesPerRow == 0) return "";

    for (size_t row = 0; row < len; row += bytesPerRow) {
        const size_t lineLen = std::min(bytesPerRow, len - row);

        // HEX column
        for (size_t i = 0; i < bytesPerRow; ++i) {
            if (i < lineLen) {
                out << toHex(data[row + i], 2) << " ";
            } else {
                out << "   ";
            }
        }

        if (withAscii) {
            out << " | ";
            for (size_t i = 0; i < lineLen; ++i) {
                uint8_t b = data[row + i];
                char c = (b >= 32 && b <= 126) ? (char)b : '.';
                out << c;
            }
        }

        if (row + bytesPerRow < len) out << "\n\r";
    }

    return out.str();
}

std::string ArgTransformer::toBinString(uint32_t value) {
    std::string bin;
    bin.reserve(32 + 3);

    // find most significant non-zero byte
    int msbByte = 3; // 4 bytes for uint32_t
    for (; msbByte > 0; --msbByte) {
        if (((value >> (msbByte * 8)) & 0xFF) != 0) break;
    }

    // display from msbByte down to byte 0
    for (int b = msbByte; b >= 0; --b) {
        uint8_t byte = (value >> (b * 8)) & 0xFF;

        for (int i = 7; i >= 0; --i) {
            bin += ((byte >> i) & 1U) ? '1' : '0';
        }

        if (b != 0) bin += ' ';
    }

    return bin;
}

std::string ArgTransformer::toAsciiString(uint32_t value) {
    std::string ascii;
    bool hasLetter = false;

    // find most significant non-zero byte
    int msbByte = 3;
    for (; msbByte > 0; --msbByte) {
        if (((value >> (msbByte * 8)) & 0xFF) != 0) break;
    }

    // build ASCII view
    for (int b = msbByte; b >= 0; --b) {
        uint8_t byte = (value >> (b * 8)) & 0xFF;

        if ((byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z')) {
            hasLetter = true;
            ascii += static_cast<char>(byte);
        }
        else if (byte >= 0x20 && byte <= 0x7E) {
            ascii += static_cast<char>(byte);
        }
        else {
            ascii += '.';
        }
    }

    // Only show ASCII if there is at least one letter
    if (!hasLetter) {
        return "";
    }

    return ascii;
}

bool ArgTransformer::parseMac(const std::string& s, std::array<uint8_t,6>& out) {
    // Accept "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"
    std::string hex;
    hex.reserve(12);
    for (char c : s) {
        if (c == ':' || c == '-') continue;
        if (!isxdigit((unsigned char)c)) return false;
        hex.push_back(c);
    }
    if (hex.size() != 12) return false;

    auto hexVal = [](char c)->uint8_t {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'a' && c <= 'f') return (uint8_t)(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return (uint8_t)(10 + c - 'A');
        return 0xFF;
    };

    for (int i=0; i<6; ++i) {
        uint8_t hi = hexVal(hex[2*i]);
        uint8_t lo = hexVal(hex[2*i+1]);
        if (hi == 0xFF || lo == 0xFF) return false;
        out[i] = (uint8_t)((hi<<4) | lo);
    }
    return true;
}

std::string ArgTransformer::ensureHttpScheme(std::string u) {
    if (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0) {
        u = "https://" + u;   // defaut https
    }
    return u;
}

std::string ArgTransformer::normalizeLines(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '\r' && i + 1 < in.size() && in[i + 1] == '\n') {
            out += "\r\n";
            ++i;
        } else if (in[i] == '\n') {
            out += "\r\n";
        } else {
            out += in[i];
        }
    }
    return out;
}

std::string ArgTransformer::extractHostFromUrl(const std::string& url) {
    size_t s = url.find("://");
    std::string host = (s == std::string::npos) ? url : url.substr(s + 3);
    size_t slash = host.find('/');
    if (slash != std::string::npos) host.erase(slash);
    return host;
}

std::vector<uint8_t> ArgTransformer::parse01List(const std::string& line) {
  std::vector<uint8_t> bits;
  auto toks = splitArgs(line);
  if (toks.size() > 1) {
    for (auto& t : toks) {
      if (t == "0" || t == "1") bits.push_back(t == "1" ? 1 : 0);
      else return {};
    }
  } else {
    for (char c : line) {
      if (c=='0' || c=='1') bits.push_back(c=='1' ? 1 : 0);
      else if (!std::isspace(static_cast<unsigned char>(c))) return {};
    }
  }
  return bits;
}

std::vector<uint8_t> ArgTransformer::packLsbFirst(const std::vector<uint8_t>& bits) {
    std::vector<uint8_t> out((bits.size() + 7) / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i] & 1) out[i / 8] |= (1u << (i % 8));
    }
    return out;
}

bool ArgTransformer::unpackLsbFirst(const std::vector<uint8_t>& bytes,
                                    size_t qty,
                                    std::vector<uint8_t>& outBits) {
    const size_t need = (qty + 7) / 8;
    if (bytes.size() < need) return false;    // too short
    outBits.resize(qty);
    for (size_t i = 0; i < qty; ++i) {
        outBits[i] = (bytes[i / 8] >> (i % 8)) & 0x1;
    }
    return true;
}

std::string ArgTransformer::toFixed2(float f) {
    std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(2); oss << f;
    return oss.str();
}

bool ArgTransformer::parsePattern(const std::string& patternRaw,
                                             std::string& outTextPattern,
                                             std::vector<uint8_t>& outHexPattern,
                                             std::vector<uint8_t>& outHexMask,
                                             bool& outIsHex) 
{
    outTextPattern.clear();
    outHexPattern.clear();
    outHexMask.clear();
    outIsHex = false;

    if (patternRaw.empty()) return false;

    // hex{ ... }
    if (patternRaw.size() >= 5 && patternRaw.rfind("hex{", 0) == 0 && patternRaw.back() == '}') {
        outIsHex = true;

        std::string inner = patternRaw.substr(4, patternRaw.size() - 5);
        auto toks = splitArgs(inner);
        if (toks.empty()) return false;

        for (auto tok : toks) {
            tok = toLower(tok);

            if (tok == "??") {
                outHexPattern.push_back(0x00);
                outHexMask.push_back(0);
                continue;
            }

            if (tok.rfind("0x", 0) == 0) tok = tok.substr(2);
            if (tok.size() != 2) return false;

            auto isHexDigit = [](char c) {
                return (c >= '0' && c <= '9') ||
                       (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
            };
            if (!isHexDigit(tok[0]) || !isHexDigit(tok[1])) return false;

            uint8_t b = parseHexOrDec("0x" + tok);
            outHexPattern.push_back(b);
            outHexMask.push_back(1);
        }

        return !outHexPattern.empty();
    }

    // text
    outIsHex = false;
    outTextPattern = decodeEscapes(patternRaw);
    return !outTextPattern.empty();
}
