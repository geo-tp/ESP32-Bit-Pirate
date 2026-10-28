#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

#include "Interfaces/IInput.h"
#include "Interfaces/ITerminalView.h"

struct BinaryBlockStats {
    float entropy;
    uint32_t printable;
    uint32_t nulls;
    uint32_t ff;
    const char* signature;
};

struct FileSignature {
    const char* name;
    const uint8_t* pattern;
    size_t length;
};

class BinaryAnalyzer {
public:
    static constexpr size_t maxFindingsPerType = 128;
    struct AnalysisResult {
        float avgEntropy;
        uint32_t totalBytes;
        uint32_t blocks;
        uint32_t printableTotal;
        uint32_t nullsTotal;
        uint32_t ffTotal;
        std::vector<std::string> foundFiles;
        std::vector<std::string> foundSecrets;
        bool findingsTruncated = false;
    };

    BinaryAnalyzer(ITerminalView& view, IInput& input);

    AnalysisResult analyze(
        uint32_t start,
        uint32_t totalSize,
        std::function<void(uint32_t address, uint8_t* buffer, uint32_t size)> fetch,
        uint32_t blockSize = 512
    );
    
    std::string formatAnalysis(const AnalysisResult& result);
private:
    IInput& terminalInput;
    ITerminalView& terminalView;

    BinaryBlockStats analyzeBlock(const uint8_t* buffer, size_t size);
    const char* detectFileSignature(const uint8_t* buf, size_t size);
    const char* detectSensitivePattern(const uint8_t* buf, size_t size);
    std::vector<std::string> extractPrintableStrings(const uint8_t* buf, size_t size, size_t minLen = 8);
    static const FileSignature knownSignatures[];
    static const size_t knownSignaturesCount;
};
