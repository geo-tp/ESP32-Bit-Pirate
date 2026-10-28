#include <unity.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Analyzers/BinaryAnalyzer.h"
#include "../Inputs/FakeInput.h"
#include "../Views/FakeTerminalView.h"

namespace binary_analyzer_tests {

struct BinaryAnalyzerFixture {
    FakeTerminalView view;
    FakeInput input;
    BinaryAnalyzer analyzer{view, input};
};

bool containsText(const std::vector<std::string>& values, const std::string& text) {
    for (const auto& value : values) {
        if (value.find(text) != std::string::npos) return true;
    }
    return false;
}

void copyMemoryWindow(const std::vector<uint8_t>& memory, uint32_t address, uint8_t* buffer, uint32_t size) {
    std::fill(buffer, buffer + size, 0);
    if (address >= memory.size()) return;

    const uint32_t available = static_cast<uint32_t>(memory.size() - address);
    const uint32_t copied = std::min(size, available);
    std::memcpy(buffer, memory.data() + address, copied);
}

void test_analyze_detects_file_signature_and_secret_across_block_overlap() {
    BinaryAnalyzerFixture fixture;
    std::vector<uint8_t> memory(128, 0x00);

    const uint8_t png[] = {0x89, 'P', 'N', 'G'};
    std::memcpy(memory.data(), png, sizeof(png));

    const char secret[] = "password=supersecret";
    std::memcpy(memory.data() + 60, secret, sizeof(secret) - 1);

    fixture.input.queueReadChar('x');
    fixture.input.queueReadChar('x');

    auto result = fixture.analyzer.analyze(
        0,
        static_cast<uint32_t>(memory.size()),
        [&](uint32_t address, uint8_t* buffer, uint32_t size) {
            copyMemoryWindow(memory, address, buffer, size);
        },
        64
    );

    TEST_ASSERT_EQUAL_UINT32(2, result.blocks);
    TEST_ASSERT_EQUAL_UINT32(128, result.totalBytes);
    TEST_ASSERT_TRUE(containsText(result.foundFiles, "PNG Image"));
    TEST_ASSERT_TRUE(containsText(result.foundSecrets, "Possible Password"));
    TEST_ASSERT_TRUE(fixture.view.contains("In progress"));
}

void test_analyze_accumulates_printable_null_and_ff_stats() {
    BinaryAnalyzerFixture fixture;
    const std::vector<uint8_t> memory = {
        0x00, 0xFF, 'A', 'B',
        0x00, 0xFF, 0x00, 0xFF
    };

    fixture.input.queueReadChar('x');
    fixture.input.queueReadChar('x');

    auto result = fixture.analyzer.analyze(
        0,
        static_cast<uint32_t>(memory.size()),
        [&](uint32_t address, uint8_t* buffer, uint32_t size) {
            copyMemoryWindow(memory, address, buffer, size);
        },
        4
    );

    TEST_ASSERT_EQUAL_UINT32(2, result.blocks);
    TEST_ASSERT_EQUAL_UINT32(8, result.totalBytes);
    TEST_ASSERT_EQUAL_UINT32(2, result.printableTotal);
    TEST_ASSERT_EQUAL_UINT32(3, result.nullsTotal);
    TEST_ASSERT_EQUAL_UINT32(3, result.ffTotal);
    TEST_ASSERT_TRUE(result.avgEntropy > 0.0f);
}

void test_analyze_counts_partial_result_when_user_stops() {
    BinaryAnalyzerFixture fixture;
    const std::vector<uint8_t> memory = {
        'B', 'u', 's', 'P',
        'i', 'r', 'a', 't',
        'e', '!', 0x00, 0xFF
    };

    fixture.input.queueReadChar('\n');

    auto result = fixture.analyzer.analyze(
        0,
        static_cast<uint32_t>(memory.size()),
        [&](uint32_t address, uint8_t* buffer, uint32_t size) {
            copyMemoryWindow(memory, address, buffer, size);
        },
        4
    );

    TEST_ASSERT_EQUAL_UINT32(1, result.blocks);
    TEST_ASSERT_EQUAL_UINT32(4, result.totalBytes);
    TEST_ASSERT_EQUAL_UINT32(4, result.printableTotal);
    TEST_ASSERT_TRUE(fixture.view.contains("[PARTIAL ANALYSIS] Stopped by User."));
}

void test_format_analysis_summarizes_empty_and_non_empty_results() {
    BinaryAnalyzerFixture fixture;

    BinaryAnalyzer::AnalysisResult empty{};
    TEST_ASSERT_TRUE(fixture.analyzer.formatAnalysis(empty).find("No data analyzed") != std::string::npos);

    BinaryAnalyzer::AnalysisResult result{};
    result.avgEntropy = 4.0f;
    result.totalBytes = 100;
    result.blocks = 2;
    result.printableTotal = 25;
    result.nullsTotal = 10;
    result.ffTotal = 5;

    const std::string formatted = fixture.analyzer.formatAnalysis(result);

    TEST_ASSERT_TRUE(formatted.find("Analysis Summary") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("Total bytes:     100") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("Blocks analyzed: 2") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("Printable chars: 25.00%") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("Null bytes:      10.00%") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("0xFF bytes:      5.00%") != std::string::npos);
}

void test_analyze_bounds_findings_and_continues_counting_all_bytes() {
    BinaryAnalyzerFixture fixture;
    const uint32_t blocks = BinaryAnalyzer::maxFindingsPerType + 50;
    for (uint32_t i = 0; i < blocks; ++i) fixture.input.queueReadChar(KEY_NONE);
    const auto result = fixture.analyzer.analyze(0, blocks * 512,
        [](uint32_t address, uint8_t* buffer, uint32_t size) {
            for (uint32_t i = 0; i < size; ++i) {
                const uint32_t position = (address + i) % 512;
                const char signature[] = "\x89PNG password=example";
                buffer[i] = position < sizeof(signature) - 1 ? signature[position] : 0;
            }
        });
    TEST_ASSERT_EQUAL_UINT32(blocks * 512, result.totalBytes);
    TEST_ASSERT_EQUAL_UINT32(BinaryAnalyzer::maxFindingsPerType, result.foundFiles.size());
    TEST_ASSERT_EQUAL_UINT32(BinaryAnalyzer::maxFindingsPerType, result.foundSecrets.size());
    TEST_ASSERT_TRUE(result.findingsTruncated);
}

}  // namespace binary_analyzer_tests

void runBinaryAnalyzerTests() {
    using namespace binary_analyzer_tests;
    RUN_TEST(test_analyze_detects_file_signature_and_secret_across_block_overlap);
    RUN_TEST(test_analyze_accumulates_printable_null_and_ff_stats);
    RUN_TEST(test_analyze_counts_partial_result_when_user_stops);
    RUN_TEST(test_format_analysis_summarizes_empty_and_non_empty_results);
    RUN_TEST(test_analyze_bounds_findings_and_continues_counting_all_bytes);
}
