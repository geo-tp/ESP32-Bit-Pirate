#include "BinaryAnalyzer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <utility>

BinaryAnalyzer::BinaryAnalyzer(ITerminalView& view, IInput& input)
    : terminalView(view), terminalInput(input) {}

const char* BinaryAnalyzer::detectSensitivePattern(const uint8_t* buf, size_t size) {
    static const char* patterns[] = {
        "-----BEGIN RSA PRIVATE KEY-----", "-----BEGIN DSA PRIVATE KEY-----", "-----BEGIN EC PRIVATE KEY-----",
        "-----BEGIN OPENSSH PRIVATE KEY-----", "-----BEGIN PGP PRIVATE KEY BLOCK-----", "-----BEGIN PRIVATE KEY-----",
        "-----BEGIN CERTIFICATE-----", "ssh-rsa", "ssh-ed25519", "ssh-dss", "ecdsa-sha2-",
        "AKIA", "eyJhbGciOi", "root:$", "WPA-PSK", "psk=",
        "api_key=", "apikey=", "secret=", "token=", "Authorization: Basic ", "Authorization: Bearer ",
        "telnetd", "password=", "pwd=", "pass:", "login:", "user:", "admin",
        "http://", "https://", "ftp://", "CONFIG_", "ENV_", "PATH=", "HOME=", "DEVICE="
    };
    static const char* labels[] = {
        "RSA Private Key", "DSA Private Key", "EC Private Key",
        "OpenSSH Private Key", "PGP Private Key", "Private Key",
        "Certificate", "SSH RSA Key", "SSH Ed25519 Key", "SSH DSA Key", "SSH ECDSA Key",
        "AWS Access Key ID", "JWT Token", "Shadow Password Hash", "WiFi WPA PSK", "WiFi PSK",
        "API Key", "API Key", "Secret", "Token", "HTTP Basic Auth", "HTTP Bearer Token",
        "Telnet Daemon String", "Password", "Password", "Password", "Login", "Username", "Admin string",
        "URL", "URL", "FTP URL", "Config Variable", "Environment Variable",
        "Path Variable", "Home Variable", "Device Variable"
    };
    static const size_t patternCount = sizeof(patterns) / sizeof(patterns[0]);


    for (size_t i = 0; i < patternCount; ++i) {
        size_t len = strlen(patterns[i]);
        for (size_t j = 0; j + len <= size; ++j) {
            bool match = true;
            for (size_t k = 0; k < len; ++k) {
                char c1 = std::tolower((unsigned char)buf[j + k]);
                char c2 = std::tolower((unsigned char)patterns[i][k]);
                if (c1 != c2) {
                    match = false;
                    break;
                }
            }
            if (match) return labels[i];
        }
    }
    return nullptr;
}

const char* BinaryAnalyzer::detectFileSignature(const uint8_t* buf, size_t size) {
    for (size_t sig = 0; sig < knownSignaturesCount; ++sig) {
        const auto& s = knownSignatures[sig];
        if (size < s.length) continue;
        for (size_t i = 0; i + s.length <= std::min(size_t(64), size); ++i) {
            if (buf[i] != s.pattern[0]) continue;
            if (memcmp(buf + i, s.pattern, s.length) == 0) return s.name;
        }
    }
    return nullptr;
}

BinaryBlockStats BinaryAnalyzer::analyzeBlock(const uint8_t* buffer, size_t size) {
    uint32_t printable = 0, nulls = 0, ff = 0, counts[256] = {0};
    float entropy = 0;
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = buffer[i];
        counts[b]++;
        if (b >= 32 && b <= 126) printable++;
        if (b == 0x00) nulls++;
        if (b == 0xFF) ff++;
    }
    for (int i = 0; i < 256; ++i) {
        if (counts[i]) {
            float p = (float)counts[i] / size;
            entropy -= p * log2(p);
        }
    }
    return {entropy, printable, nulls, ff, detectFileSignature(buffer, size)};
}

BinaryAnalyzer::AnalysisResult BinaryAnalyzer::analyze(
    uint32_t start,
    uint32_t totalSize,
    std::function<void(uint32_t address, uint8_t* buffer, uint32_t size)> fetch,
    uint32_t blockSize
) {
    const uint32_t overlap = 32;
    uint8_t buffer[blockSize + overlap];

    uint32_t printableTotal = 0, nullsTotal = 0, ffTotal = 0, blocks = 0;
    float entropySum = 0;
    std::vector<std::string> foundFiles, foundSecrets;
    bool findingsTruncated = false;
    uint32_t totalBlocks = (totalSize - start) / blockSize;
    size_t dotIntervalSz = std::max<size_t>(totalBlocks / 30, (size_t)1);
    uint32_t dotInterval = (uint32_t)dotIntervalSz;

    terminalView.print("In progress");

    for (uint32_t addr = start; addr < totalSize; addr += blockSize) {
        uint32_t readAddr = (addr >= overlap) ? (addr - overlap) : 0;
        uint32_t readSize = (addr >= overlap) ? (blockSize + overlap) : (blockSize + addr);
        fetch(readAddr, buffer, readSize);

        const uint8_t* blockData = buffer + (addr - readAddr);

        BinaryBlockStats stats = analyzeBlock(blockData, blockSize);
        entropySum += stats.entropy;
        printableTotal += stats.printable;
        nullsTotal += stats.nulls;
        ffTotal += stats.ff;

        if (stats.signature && foundFiles.size() < maxFindingsPerType) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << addr;
            ss << " → " << stats.signature;
            foundFiles.push_back(ss.str());
        } else if (stats.signature) {
            findingsTruncated = true;
        }

        const char* sensitive = detectSensitivePattern(buffer, readSize);
        if (sensitive && foundSecrets.size() < maxFindingsPerType) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << addr;
            ss << " → Possible " << sensitive;
            foundSecrets.push_back(ss.str());
        } else if (sensitive) {
            findingsTruncated = true;
        }

        ++blocks;

        if (blocks % dotInterval == 0) {
            terminalView.print(".");
        }

        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\n[PARTIAL ANALYSIS] Stopped by User.\n");
            break;
        };
    }

    float avgEntropy = (blocks > 0) ? (entropySum / blocks) : 0;
    return {avgEntropy, blocks * blockSize, blocks, printableTotal, nullsTotal, ffTotal,
            std::move(foundFiles), std::move(foundSecrets), findingsTruncated};
}

std::string BinaryAnalyzer::formatAnalysis(const AnalysisResult& result) {
    if (result.totalBytes == 0) return "❌ No data analyzed.\n";

    float printablePct = 100.0f * result.printableTotal / result.totalBytes;
    float nullsPct     = 100.0f * result.nullsTotal     / result.totalBytes;
    float ffPct        = 100.0f * result.ffTotal        / result.totalBytes;
    uint32_t dataBytes = result.totalBytes - (result.nullsTotal + result.ffTotal);
    float dataPct      = 100.0f * dataBytes / result.totalBytes;

    float normalizedEntropy = result.avgEntropy / 8.0f;
    int barLength = 20;
    int filled = std::round(normalizedEntropy * barLength);

    std::string bar = "[";
    for (int i = 0; i < barLength; ++i)
        bar += (i < filled) ? '#' : '.';
    bar += "]";

    std::string interpretation;
    if (normalizedEntropy >= 0.95f)
        interpretation = "→ likely encrypted/compressed";
    else if (normalizedEntropy >= 0.85f)
        interpretation = "→ mostly compressed";
    else if (normalizedEntropy >= 0.65f)
        interpretation = "→ mixed content";
    else if (normalizedEntropy >= 0.4f)
        interpretation = "→ partially structured";
    else if (normalizedEntropy >= 0.2f)
        interpretation = "→ contains padding";
    else
        interpretation = "→ likely empty";

    char line[512];
    snprintf(line, sizeof(line),
        "\n\n\r📊 Analysis Summary:\n\r"
        " • Total bytes:     %u\n\r"
        " • Blocks analyzed: %u\n\r"
        " • Avg. entropy:    %.2f / 8.00\n\r"
        " • Entropy bar:     %s %s\n\r"
        " • Printable chars: %.2f%%\n\r"
        " • Null bytes:      %.2f%%\n\r"
        " • 0xFF bytes:      %.2f%%\n\r"
        " • Data bytes:      %.2f%%\r",
        result.totalBytes,
        result.blocks,
        result.avgEntropy,
        bar.c_str(),
        interpretation.c_str(),
        printablePct,
        nullsPct,
        ffPct,
        dataPct
    );

    std::string summary(line);
    if (result.findingsTruncated)
        summary += "\nResult list limited to the first " + std::to_string(maxFindingsPerType) +
                   " matches per type to conserve RAM. Statistics cover all analyzed bytes.\n";
    return summary;
}

std::vector<std::string> BinaryAnalyzer::extractPrintableStrings(const uint8_t* buf, size_t size, size_t minLen) {
    std::vector<std::string> strings;
    std::string current;
    for (size_t i = 0; i < size; ++i) {
        char c = buf[i];
        if (c >= 32 && c <= 126) {
            current += c;
        } else {
            if (current.length() >= minLen) {
                strings.push_back(current);
            }
            current.clear();
        }
    }
    if (current.length() >= minLen) strings.push_back(current);
    return strings;
}

const FileSignature BinaryAnalyzer::knownSignatures[] = {
    // Executables / Boot
    { "ELF Executable",          (const uint8_t*)"\x7F""ELF", 4 },
    { "U-Boot uImage",           (const uint8_t*)"\x27\x05\x19\x56", 4 },
    { "Device Tree Blob (FDT)",  (const uint8_t*)"\xD0\x0D\xFE\xED", 4 },
    { "TRX Firmware Header",     (const uint8_t*)"HDR0", 4 },

    // Archives / Compression
    { "GZIP Archive",            (const uint8_t*)"\x1F\x8B", 2 },
    { "ZIP Archive",             (const uint8_t*)"\x50\x4B\x03\x04", 4 },
    { "7z Archive",              (const uint8_t*)"\x37\x7A\xBC\xAF\x27\x1C", 6 },
    { "XZ Compressed",           (const uint8_t*)"\xFD\x37\x7A\x58\x5A\x00", 6 },
    { "LZMA compressed",         (const uint8_t*)"\x5D\x00\x00", 3 },
    { "LZ4 Frame",               (const uint8_t*)"\x04\x22\x4D\x18", 4 },
    { "BZIP2 Compressed",        (const uint8_t*)"BZh", 3 },
    { "Zstandard Compressed",    (const uint8_t*)"\x28\xB5\x2F\xFD", 4 },
    { "LZO Compressed",          (const uint8_t*)"\x89LZO\x00\x0D\x0A\x1A\x0A", 9 },
    { "RAR Archive",             (const uint8_t*)"Rar!\x1A\x07\x00", 7 },
    { "MS Cabinet (CAB)",        (const uint8_t*)"MSCF", 4 },
    { "CPIO Archive (newc)",     (const uint8_t*)"070701", 6 },

    // File systems
    { "SquashFS (little-endian)", (const uint8_t*)"hsqs", 4 },
    { "SquashFS (big-endian)",   (const uint8_t*)"sqsh", 4 },
    { "CRAMFS (little-endian)",  (const uint8_t*)"\x45\x3D\xCD\x28", 4 },
    { "CRAMFS (big-endian)",     (const uint8_t*)"\x28\xCD\x3D\x45", 4 },
    { "JFFS2",                   (const uint8_t*)"\x85\x19\x03\x20", 4 },
    { "UBI/UBIFS",               (const uint8_t*)"\x55\x42\x49\x23", 4 },
    { "Ext2/3/4 Superblock",     (const uint8_t*)"\x53\xEF", 2 }, // offset 0x438 en réalité

    // Images
    { "PNG Image",               (const uint8_t*)"\x89PNG", 4 },
    { "JPEG Image",              (const uint8_t*)"\xFF\xD8\xFF", 3 },
    { "GIF Image",               (const uint8_t*)"GIF8", 4 },
    { "BMP Image",               (const uint8_t*)"BM", 2 },

    // Documents
    { "PDF Document",            (const uint8_t*)"%PDF-", 5 },
    { "RTF Document",            (const uint8_t*)"{\\rtf", 5 },
    { "SQLite 3 DB",             (const uint8_t*)"SQLite format 3", 16 },

    // Audio / Video
    { "MP3 (ID3)",               (const uint8_t*)"ID3", 3 },
    { "WAV Audio",               (const uint8_t*)"RIFF", 4 }, // + "WAVE" after 8 bytes
    { "AVI Video",               (const uint8_t*)"RIFF", 4 }, // + "AVI " after 8 bytes

    // Divers
    { "TAR Archive (ustar)",     (const uint8_t*)"ustar", 5 },
    { "RAFFS",                   (const uint8_t*)"\x52\x41\x46\x46\x53", 5 },
};
const size_t BinaryAnalyzer::knownSignaturesCount = sizeof(BinaryAnalyzer::knownSignatures) / sizeof(FileSignature);
