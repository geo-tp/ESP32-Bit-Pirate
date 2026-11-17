
/*

############################################################################################################################################
#                                                                                                                                          #
#                                                                                                                                          #
#                                 This file was adapted from https://github.com/probonopd/MakeHex/.                                        #
#                               It takes an IRP string and return the raw IR sequence. By John Fine                                        #
#                                                                                                                                          #
#                                                                                                                                          #
############################################################################################################################################

*/

// Copyright 2005 John S. Fine

// You may use, copy, modify and/or distribute this program for private or commercial use provided that:
// 1) You do not hold me responsible for any damage or negative consequences resulting from those activities.
// 2) You include this copyright notice and disclaimer in any copy of any part or all of this program.
// I do not provide any warranty of the correctness, safety, or suitibility of this program for any purpose.
// If you do not agree to these conditions, you have no permission to use, copy, modify or distribute this program.

// Ultimate Remote additions: see UltimateRemote.LICENSE.
#include "MakeHex.h"



std::vector<float> encodeRemoteCommand(const InfraredCommand& cmd, const char* protocolString, int& frequency) {

    if (!protocolString || !*protocolString) return {};
    frequency = 38000;

    // Adapted from
    // https://github.com/probonopd/MakeHex/tree/master
    // By John Fine

    // Zenith uses a nested binary bit specification and a variable number of
    // function bits. Historical MakeHex cannot change the bit specification
    // midway through a frame; encode that one protocol directly from its IRP:
    // {40k,520,msb}<1,-10|1,-1,1,-8>(S:1,<1:2|2:2>(F:D),-90m)*
    // Reference: Harctoolbox IrpProtocols.xml, Zenith.
    if (strcmp(protocolString, "zenith") == 0) {
        const int bits = cmd.getDevice();
        const int style = cmd.getSubdevice();
        if (bits < 5 || bits > 8 || style < 0 || style > 1 ||
            cmd.getFunction() < 0 || cmd.getFunction() >= (1 << bits)) return {};
        frequency = 40000;
        std::vector<float> out;
        auto emitOuter = [&](bool one) {
            if (one) {
                out.insert(out.end(), {520.f, 520.f, 520.f, 4160.f});
            } else {
                out.insert(out.end(), {520.f, 5200.f});
            }
        };
        emitOuter(style != 0);
        for (int i = bits - 1; i >= 0; --i) {
            // Nested 2-bit symbols: 0 -> 01, 1 -> 10 (MSB first).
            const bool bit = (cmd.getFunction() >> i) & 1;
            emitOuter(bit);
            emitOuter(!bit);
        }
        out.back() += 90000.f;
        return out;
    }

    // Dedicated Apple Remote encoder. Apple is NOT regular NEC1: its 32-bit
    // payload includes a fixed 0x87 byte, a 1-bit check, 7-bit function and
    // an 8-bit remote Pair ID. Source: Harctoolbox IrpProtocols.xml (Apple).
    // The RemoteCommand fields are: D=device (normally 238), S=Pair ID,
    // F=command. Built-in Apple entries use Pair ID 0 (see Database.h).
    if (strcmp(protocolString, "apple") == 0) {
        if (cmd.getDevice() < 0 || cmd.getDevice() > 255 ||
            cmd.getSubdevice() < 0 || cmd.getSubdevice() > 255 ||
            cmd.getFunction() < 0 || cmd.getFunction() > 127) return {};
        const uint32_t device = static_cast<uint8_t>(cmd.getDevice());
        const uint32_t pairId = static_cast<uint8_t>(cmd.getSubdevice());
        const uint32_t function = static_cast<uint8_t>(cmd.getFunction());
        const auto countBits = [](uint32_t n) {
            unsigned count = 0;
            while (n) { count += n & 1u; n >>= 1; }
            return count;
        };
        const uint32_t check = 1u - ((countBits(device) + countBits(135u) +
                                      countBits(function) + countBits(pairId)) & 1u);
        const uint32_t payload = device | (135u << 8) | (check << 16) |
                                 (function << 17) | (pairId << 24);
        frequency = 38400;
        std::vector<float> result;
        result.reserve(68);
        result.push_back(16.f * 564.f);
        result.push_back(8.f * 564.f);
        for (unsigned b = 0; b < 32; ++b) {
            result.push_back(564.f);
            result.push_back((payload & (uint32_t(1) << b)) ? 3.f * 564.f : 564.f);
        }
        result.push_back(564.f);
        // NEC1-compatible 108ms frame period; include trailing SPACE so
        // the raw sender always receives an even number of timings.
        float elapsed = 0.f;
        for (float duration : result) elapsed += duration;
        result.push_back(108000.f - elapsed);
        return result;
    }

    // NEC42 / NEC42ext: 42-bit NEC frames as used by the existing Flipper
    // parsed-file encoder. Builder mapping: D=low 13 address bits,
    // S=high 13 address bits (only for NEC42ext), F=command (8/16-bit).
    if (strcmp(protocolString, "NEC42") == 0 ||
        strcmp(protocolString, "NEC42ext") == 0) {
        const bool extended = strcmp(protocolString, "NEC42ext") == 0;
        if (cmd.getDevice() < 0 || cmd.getDevice() > 8191 ||
            (extended && (cmd.getSubdevice() < -1 || cmd.getSubdevice() > 8191)) ||
            cmd.getFunction() < 0 || cmd.getFunction() > (extended ? 32767 : 255)) return {};
        frequency = 38000;
        const uint64_t address = static_cast<uint16_t>(cmd.getDevice()) |
            ((extended && cmd.getSubdevice() >= 0) ?
                (static_cast<uint64_t>(cmd.getSubdevice()) << 13) : 0ULL);
        const uint64_t f = static_cast<uint16_t>(cmd.getFunction());
        const uint64_t bits = extended ? (address | (f << 26)) :
            (address | ((~address & 8191ULL) << 13) | (f << 26) |
             ((~f & 255ULL) << 34));
        std::vector<float> result{9000.f, 4500.f};
        result.reserve(88);
        for (unsigned i = 0; i < 42; ++i) {
            result.push_back(560.f);
            result.push_back((bits & (1ULL << i)) ? 1690.f : 560.f);
        }
        result.push_back(560.f);
        float elapsed = 0;
        for (float t : result) elapsed += t;
        result.push_back(145000.f - elapsed);
        return result;
    }

    // Built-in Samsung32 and Sony SIRC variants share their physical format
    // with the existing Flipper .ir support, but are represented by D/S/F in
    // the built-in DB / IR Builder rather than an already packed payload.
    if (strcmp(protocolString, "Samsung32") == 0) {
        if (cmd.getDevice() < 0 || cmd.getDevice() > 255 ||
            cmd.getFunction() < 0 || cmd.getFunction() > 255) return {};
        frequency = 38000;
        const uint32_t d = static_cast<uint8_t>(cmd.getDevice());
        const uint32_t f = static_cast<uint8_t>(cmd.getFunction());
        const uint32_t bits = d | (d << 8) | (f << 16) | ((f ^ 255u) << 24);
        std::vector<float> result{4500.f, 4500.f};
        result.reserve(68);
        for (unsigned bit = 0; bit < 32; ++bit) {
            result.push_back(550.f);
            result.push_back((bits & (uint32_t(1) << bit)) ? 1650.f : 550.f);
        }
        result.push_back(550.f);
        float elapsed = 0;
        for (float t : result) elapsed += t;
        result.push_back(108000.f - elapsed);
        return result;
    }
    if (strcmp(protocolString, "SIRC") == 0 || strcmp(protocolString, "SIRC15") == 0 ||
        strcmp(protocolString, "SIRC20") == 0) {
        const bool longAddress = strcmp(protocolString, "SIRC15") == 0;
        const bool twenty = strcmp(protocolString, "SIRC20") == 0;
        const unsigned bits = twenty ? 20u : (longAddress ? 15u : 12u);
        const int maxDevice = longAddress ? 255 : 31;
        if (cmd.getDevice() < 0 || cmd.getDevice() > maxDevice ||
            cmd.getFunction() < 0 || cmd.getFunction() > 127 ||
            (twenty && (cmd.getSubdevice() < 0 || cmd.getSubdevice() > 255))) return {};
        frequency = 40000;
        uint32_t value = static_cast<uint8_t>(cmd.getFunction()) |
                         (static_cast<uint32_t>(cmd.getDevice()) << 7);
        if (twenty) value |= static_cast<uint32_t>(cmd.getSubdevice()) << 12;
        std::vector<float> result;
        result.reserve((2 + bits * 2) * 3);
        for (unsigned repeat = 0; repeat < 3; ++repeat) {
            const size_t start = result.size();
            result.insert(result.end(), {2400.f, 600.f});
            for (unsigned b = 0; b < bits; ++b) {
                result.push_back((value & (uint32_t(1) << b)) ? 1200.f : 600.f);
                result.push_back(600.f);
            }
            float elapsed = 0.f;
            for (size_t i = start; i < result.size(); ++i) elapsed += result[i];
            // Whole Sony frame repeats about every 45ms; the last mark has
            // a single combined lead-out SPACE, not an extra phantom mark.
            result.back() += 45000.f - elapsed;
        }
        return result;
    }

    char irp[1024] = ""; // IRP string, will contains all necessary infos to generate the IR sequence

    // Device
    if (cmd.getSubdevice() >= 0)
        snprintf(irp, sizeof(irp), "Device=%d.%d\nFunction=%d\n", cmd.getDevice(), cmd.getSubdevice(), cmd.getFunction());
    else
        snprintf(irp, sizeof(irp), "Device=%d\nFunction=%d\n", cmd.getDevice(), cmd.getFunction());

    // Protocol
    int p = -1;
    for (size_t i = 0; i < sizeof(protocolDefinitions) / sizeof(protocolDefinitions[0]); i++) {
        if (strcmp(protocolString, protocolDefinitions[i].name) == 0) {
            p = i;
            break;
        }
    }
    if (p < 0) {
        // Protocol not found, try for special protocols
        int M = 0;
        int L = 0;
        char tempProt[100];
        strncpy(tempProt, protocolString, sizeof(tempProt));
        tempProt[sizeof(tempProt) - 1] = '\0';
        for (int i = 0; i < strlen(tempProt); i++)
            tempProt[i] = toupper(static_cast<unsigned char>(tempProt[i])); // Convert to uppercase

        int consumed = 0;
        if (sscanf(tempProt, "RC6-%d-%d%n", &M, &L, &consumed) == 2) {
            if (tempProt[consumed] || M < 0 || M > 7 || L < 20 || L > 48) return {};
            char temp[512];
            snprintf(temp, sizeof(temp), "Define M=%d\nDefine L=%d\n", M, L);
            strncat(irp, temp, sizeof(irp) - strlen(irp) - 1);
            protocolString = "rc6-M-L";
        } else if (strcmp("NEC", tempProt) == 0) {
            protocolString = "nec2";
        } else if (strcmp("NECX", tempProt) == 0) {
            protocolString = "NECx2";
        }

        // Search again for protocol
        for (size_t i = 0; i < sizeof(protocolDefinitions) / sizeof(protocolDefinitions[0]); i++) {
            if (strcmp(protocolString, protocolDefinitions[i].name) == 0) {
                p = i;
                break;
            }
        }

        // Never send an unrelated NEC2 frame for an unsupported protocol.
        if (p < 0) return {};
    }

    strncat(irp, protocolDefinitions[p].def, sizeof(irp) - strlen(irp) - 1);

    // Frequency
    const char* freqStr = strstr(protocolDefinitions[p].def, "Frequency=");
    if (freqStr != nullptr) {
        sscanf(freqStr, "Frequency=%d", &frequency);
        // Keep carrier frequency in Hz until the hardware interface.
    }

    // Validate IRP string
    IRP Irp;
    if (!Irp.readIrpString(irp)) {
        // Bad IRP
        return {};
    }

    // Set values
    Irp.m_value['D' - 'A'] = cmd.getDevice();
    Irp.m_value['S' - 'A'] = cmd.getSubdevice();
    Irp.m_value['F' - 'A'] = cmd.getFunction();
    Irp.m_value['N' - 'A'] = -1;

    // Generate IR sequence
    int s, r;
    std::vector<float> result;
    Irp.generate(&s, &r, result);
    // Reject malformed generated sequences rather than passing them to LIRC.
    if (result.empty() || (result.size() & 1u) || result.size() > 2048) return {};
    for (float duration : result) if (!std::isfinite(duration) || duration <= 0) return {};
    return result;
}

unsigned int IRP::reverse(unsigned int Number) {
    Number = ((Number & 0x55555555) << 1) + ((Number >> 1) & 0x55555555);
    Number = ((Number & 0x33333333) << 2) + ((Number >> 2) & 0x33333333);
    Number = ((Number & 0xF0F0F0F)  << 4) + ((Number >> 4) & 0xF0F0F0F);
    Number = ((Number & 0xFF00FF)   << 8) + ((Number >> 8) & 0xFF00FF);
    return (Number >> 16) + (Number << 16);
}

IRP::IRP() :
    m_frequency(38400),
    m_timeBase(1),
    m_messageTime(0),
    m_prefix(0),
    m_suffix(0),
    m_rPrefix(0),
    m_rSuffix(0),
    m_msb(false),
    m_form(0),
    m_bitGroup(2)
{
    memset(m_value, 0, sizeof(m_value));
    memset(m_def, 0, sizeof(m_def));
    memset(m_device, -1, sizeof(m_device));
    memset(m_functions, -1, sizeof(m_functions));
    memset(m_digits, 0, sizeof(m_digits));
    m_mask[0] = 0;
    for (int ndx = 1; ndx < 33; ++ndx)
        m_mask[ndx] = 2 * m_mask[ndx - 1] + 1;
}

IRP::~IRP() {
    for (int i = 0; i < 26; ++i) {
        if (m_def[i]) {
            free((void *)m_def[i]);
        }
    }
    for (int i = 0; i < 16; ++i) {
        if (m_digits[i]) {
            free((void *)m_digits[i]);
        }
    }
    free((void *)m_prefix);
    free((void *)m_suffix);
    free((void *)m_rPrefix);
    free((void *)m_rSuffix);
    free((void *)m_form);
}

bool IRP::match(const char* master) {
    int len = strlen(master);
    m_next = m_bufr + len;
    return 0 == memcmp(m_bufr, master, len);
}
void IRP::setDigit(int d) {
    m_digits[d] = copy();
    while (d >= m_bitGroup) {
        m_bitGroup <<= 1;
    }
}

bool IRP::readIrpString(char* str) {
    char* line = strtok(str, "\n");
    while (line != NULL && line[0] != 0) {
        strcpy(m_bufr, line);
        line = m_bufr;

        Value val;

        // Convert to upper case, compress out blanks and discard comments
        for (char* l2 = line; *l2 && *l2 != '\'' && *l2 != '\n'; l2++) {
            if (*l2 != ' ' && *l2 != '\t')
                *(line++) = toupper(*l2);
        }
        *line = 0;

        // Recognise some Keywords
        if (match("FREQUENCY=")) {
            parseVal(val, const_cast<char*&>(m_next));
            m_frequency = val.m_val;
        } else if (match("TIMEBASE=")) {
            parseVal(val, const_cast<char*&>(m_next));
            m_timeBase = val.m_val;
        } else if (match("MESSAGETIME=")) {
            parseVal(val, const_cast<char*&>(m_next));
            if (val.m_bits == 0)
                val.m_val *= m_timeBase;
            m_messageTime = val.m_val;
        } else if (m_bufr[0] >= '0' && m_bufr[0] <= '9' && m_bufr[1] == '=') {
            m_next = m_bufr + 2;
            setDigit(m_bufr[0] - '0');
        } else if (m_bufr[0] == '1' && m_bufr[1] >= '0' && m_bufr[1] <= '5' && m_bufr[2] == '=') {
            m_next = m_bufr + 3;
            setDigit(m_bufr[1] + (10 - '0'));
        } else if (match("ZERO=")) {
            setDigit(0);
        } else if (match("ONE=")) {
            setDigit(1);
        } else if (match("TWO=")) {
            setDigit(2);
        } else if (match("THREE=")) {
            setDigit(3);
        } else if (match("PREFIX=")) {
            m_prefix = copy();
        } else if (match("SUFFIX=")) {
            m_suffix = copy();
        } else if (match("R-PREFIX=")) {
            m_rPrefix = copy();
        } else if (match("R-SUFFIX=")) {
            m_rSuffix = copy();
        } else if (match("FIRSTBIT=MSB")) {
            m_msb = true;
        } else if (match("FORM=")) {
            m_form = copy();
        } else if (match("DEFINE") || match("DEFAULT")) {
            if (m_next[1] == '=') {
                m_next += 2;
                m_def[m_next[-2] - 'A'] = copy();
            } else if (m_next[0] == '=' && m_next[2] == 'A' && m_next[3] == 'S') {
                m_next += 4;
                m_def[m_next[-3] - 'A'] = copy();
            } else if (m_next[1] == 'A' && m_next[2] == 'S') {
                m_next += 3;
                m_def[m_next[-3] - 'A'] = copy();
            }
        } else if (match("DEVICE=")) {
            getPair(m_device);
        } else if (match("FUNCTION=")) {
            getPair(m_functions);
            if (m_next[0] == '.' && m_next[1] == '.') {
                m_next += 2;
                getPair(m_functions + 2);
            }
        }

        line = strtok(NULL, "\n");
    }

    if (m_device[1] >= 0) {
        free((void*)m_def['S' - 'A']);
        m_def['S' - 'A'] = nullptr;
    }
    if (m_functions[1] >= 0) {
        free((void*)m_def['N' - 'A']);
        m_def['N' - 'A'] = nullptr;
    }

    return true;
}

void IRP::getPair(int* result) {
    for (int nIndex = 0; nIndex < 2; nIndex++) {
        int num = 0;
        for (; *m_next; m_next++) {
            if (*m_next >= '0' && *m_next <= '9')
                num = num * 10 + *m_next - '0';
            else
                break;
        }
        result[nIndex] = num;
        if (m_next[0] != '.' || m_next[1] < '0' || m_next[1] > '9')
            break;
        m_next++;
    }
}

char* IRP::copy() {
    return strdup(m_next);
}

void IRP::parseVal(Value& result, char*& in, int prec) {
    if (*in >= 'A' && *in <= 'Z') {
        int ndx = *(in++) - 'A';
        const char* in2 = m_def[ndx];
        if (in2) {
            parseVal(result, const_cast<char*&>(in2), 0);
            if (*in2) {
                // error
            }
        } else {
            result.m_val = m_value[ndx];
            result.m_bits = 0;
        }
    } else if (*in >= '0' && *in <= '9') {
        result.m_bits = 0;
        result.m_val = 0.0;
        do {
            result.m_val = result.m_val * 10 + *(in++) - '0';
        } while (*in >= '0' && *in <= '9');
    } else switch (*in) {
        case '-':
            ++in;
            parseVal(result, in, 1);
            result.m_val = -result.m_val;
            if (result.m_bits > 0)
                result.m_bits = 0;
            break;
        case '~':
            ++in;
            parseVal(result, in, 1);
            result.m_val = -(result.m_val + 1);
            if (result.m_bits > 0)
                result.m_val = (double)(((int)result.m_val) & m_mask[result.m_bits]);
            break;
        case '(':
            ++in;
            parseVal(result, in, 0);
            if (*in == ')') {
                ++in;
            } else {
                // error
            }
            break;
        default:
            // error
            break;
    }

    if (*in == 'M') {
        result.m_val *= 1000;
        result.m_bits = -1;
        ++in;
    } else if (*in == 'U') {
        result.m_bits = -1;
        ++in;
    }

    for (;;) {
        Value v2;
        if (prec < 2) {
            if (*in == '*') {
                ++in;
                parseVal(v2, in, 2);
                result.m_val *= v2.m_val;
                if (result.m_bits > 0)
                    result.m_bits = 0;
                continue;
            }
        }
        if (prec < 1) {
            switch (*in) {
                case '+':
                    ++in;
                    parseVal(v2, in, 1);
                    result.m_val += v2.m_val;
                    if (result.m_bits > 0)
                        result.m_bits = 0;
                    continue;
                case '-':
                    ++in;
                    parseVal(v2, in, 1);
                    result.m_val -= v2.m_val;
                    if (result.m_bits > 0)
                        result.m_bits = 0;
                    continue;
                case '^':
                    ++in;
                    parseVal(v2, in, 1);
                    result.m_val = ((int)result.m_val) ^ ((int)v2.m_val);
                    if (result.m_bits > 0 && (v2.m_bits <= 0 || v2.m_bits > result.m_bits))
                        result.m_bits = v2.m_bits;
                    continue;
            }
        }
        if (prec < 3) {
            if (*in == ':') {
                ++in;
                parseVal(v2, in, 3);
                result.m_bits = v2.m_val;
                if (*in == ':') {
                    ++in;
                    parseVal(v2, in, 3);
                    result.m_val = (double)(((int)result.m_val) >> ((int)v2.m_val));
                }
                if (result.m_bits < 0) {
                    result.m_bits = -result.m_bits;
                    result.m_val = (double)(reverse((int)result.m_val) >> (32 - result.m_bits));
                }
                result.m_val = (double)(((int)result.m_val) & m_mask[result.m_bits]);
                continue;
            }
        }
        break;
    }
}

void IRP::generate(int* s, int* r, std::vector<float>& raw) {
    m_hex.clear();
    m_cumulative = 0.0;
    m_pendingBits = (m_msb ? 1 : m_bitGroup);
    if (!m_form) { raw.clear(); *s = *r = 0; return; }
    int Single = genHex(m_form);
    if (m_cumulative < m_messageTime)
        genHex(m_cumulative - m_messageTime);
    if (m_hex.size() & 1)
        genHex(-1.0);
    if (Single < 0)
        Single = m_hex.size();
    Single >>= 1;

    raw = m_hex;

    *s = Single;
    *r = m_hex.size() / 2 - Single;
    return;
}

int IRP::genHex(const char* Pattern) {
    int Result = -1;
    if (!Pattern) return Result;
    if (*Pattern == ';') {
        Result = 0; // Single section is empty
        Pattern++;
    }
    while (*Pattern) {
        if (*Pattern == '*') {
            genHex((Result >= 0 && m_rPrefix) ? m_rPrefix : m_prefix);
            Pattern++;
        } else if (*Pattern == '_') {
            genHex((Result >= 0 && m_rSuffix) ? m_rSuffix : m_suffix);
            Pattern++;
            if (m_cumulative < m_messageTime) {
                genHex(m_cumulative - m_messageTime);
            }
        } else if (*Pattern == '^') {
            Pattern++;

            Value val;
            parseVal(val, const_cast<char*&>(Pattern));
            if (val.m_bits == 0)
                val.m_val *= m_timeBase;

            if (m_cumulative < val.m_val) {
                genHex(m_cumulative - val.m_val);
            }
        } else {
            Value val;
            parseVal(val, const_cast<char*&>(Pattern));

            if (val.m_bits == 0)
                val.m_val *= m_timeBase;
            if (val.m_bits <= 0) {
                genHex(val.m_val);
            } else {
                int Number = (int)(val.m_val);
                if (m_msb)
                    Number = reverse(Number) >> (32 - val.m_bits);
                while (--val.m_bits >= 0) {
                    if (m_msb) {
                        m_pendingBits = (m_pendingBits << 1) + (Number & 1);
                        if (m_pendingBits & m_bitGroup) {
                            genHex(m_digits[m_pendingBits - m_bitGroup]);
                            m_pendingBits = 1;
                        }
                    } else {
                        m_pendingBits = (m_pendingBits >> 1) + (Number & 1) * m_bitGroup;
                        if (m_pendingBits & 1) {
                            genHex(m_digits[m_pendingBits >> 1]);
                            m_pendingBits = m_bitGroup;
                        }
                    }
                    Number >>= 1;
                }
            }
        }
        if (*Pattern == ';') {
            if (m_cumulative < m_messageTime) {
                genHex(m_cumulative - m_messageTime);
            }
            if (m_hex.size() & 1) {
                genHex(-1.0);
            }
            Result = m_hex.size();
            m_cumulative = 0.0;
        } else if (*Pattern != ',')
            break;
        Pattern++;
    }
    return Result;
}

void IRP::genHex(float number) {
    if (number == 0.0)
        return;
    int nHex = m_hex.size();
    if (number > 0) {
        m_cumulative += number;
        if (nHex & 1)
            m_hex[nHex - 1] += number;
        else
            m_hex.push_back(number);
    } else if (nHex) {
        m_cumulative -= number;
        if (nHex & 1)
            m_hex.push_back(-number);
        else
            m_hex[nHex - 1] -= number;
    }
}
