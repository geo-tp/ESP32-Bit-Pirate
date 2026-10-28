#include <unity.h>
#include "Analyzers/I2cFrequencyAnalyzer.h"

namespace i2c_frequency_analyzer_tests {

// Each test feeds measured levels, including an extra falling edge to close
// the final complete clock period. One tick is 50 ns.
void clocks(I2cFrequencyAnalyzer& analyzer, unsigned count, uint32_t low, uint32_t high) {
    for (unsigned i = 0; i < count; ++i) {
        analyzer.addPulse(low, false);
        analyzer.addPulse(high, true);
    }
    analyzer.addPulse(1, false);
    analyzer.endBurst();
}

void test_frequency_measures_standard_fast_and_fast_plus_clocks() {
    for (uint32_t period : {200u, 50u, 20u}) {
        I2cFrequencyAnalyzer analyzer;
        clocks(analyzer, 18, period * 3 / 5, period * 2 / 5);
        const auto result = analyzer.result(20000000);
        TEST_ASSERT_TRUE(result.reliable);
        TEST_ASSERT_EQUAL_UINT32(18, result.capturedCycles);
        TEST_ASSERT_EQUAL_UINT32(18, result.acceptedCycles);
        TEST_ASSERT_FLOAT_WITHIN(0.1, 20000000.0 / period, result.frequencyHz);
        TEST_ASSERT_FLOAT_WITHIN(0.001, period * 0.05, result.periodUs);
        TEST_ASSERT_FLOAT_WITHIN(0.001, period * 0.03, result.lowUs);
        TEST_ASSERT_FLOAT_WITHIN(0.001, period * 0.02, result.highUs);
    }
}

void test_frequency_excludes_idle_gaps_stretching_and_short_glitches() {
    I2cFrequencyAnalyzer analyzer;
    clocks(analyzer, 30, 30, 20); // 400 kHz, asymmetric duty cycle
    clocks(analyzer, 2, 30, 10000); // long HIGH between transfers
    clocks(analyzer, 3, 10000, 20); // clock stretching
    clocks(analyzer, 1, 2, 2); // short glitch
    const auto result = analyzer.result(20000000);
    TEST_ASSERT_TRUE(result.reliable);
    TEST_ASSERT_EQUAL_UINT32(36, result.capturedCycles);
    TEST_ASSERT_EQUAL_UINT32(30, result.acceptedCycles);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 400000, result.frequencyHz);
}

void test_frequency_does_not_join_bursts_or_use_truncated_tails() {
    I2cFrequencyAnalyzer analyzer;
    for (unsigned i = 0; i < 10; ++i) {
        analyzer.addPulse(700, true); // capture starts during HIGH
        analyzer.addPulse(120, false);
        analyzer.addPulse(80, true);
        analyzer.addPulse(120, false); // closes one complete cycle
        analyzer.addPulse(20000, true); // terminal idle, no closing edge
        analyzer.addPulse(0, false); // hardware EOF
    }
    const auto result = analyzer.result(20000000);
    TEST_ASSERT_TRUE(result.reliable);
    TEST_ASSERT_EQUAL_UINT32(10, result.capturedCycles);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 100000, result.frequencyHz);
}

void test_frequency_rejects_idle_bus_insufficient_cycles_and_mixed_speeds() {
    I2cFrequencyAnalyzer analyzer;
    TEST_ASSERT_FALSE(analyzer.result(20000000).reliable);
    clocks(analyzer, 7, 120, 80);
    TEST_ASSERT_FALSE(analyzer.result(20000000).reliable);
    clocks(analyzer, 1, 120, 80);
    TEST_ASSERT_TRUE(analyzer.result(20000000).reliable);
    clocks(analyzer, 8, 30, 20);
    const auto result = analyzer.result(20000000);
    TEST_ASSERT_FALSE(result.reliable);
    TEST_ASSERT_EQUAL_UINT32(16, result.capturedCycles);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, result.frequencyHz);
    TEST_ASSERT_FALSE(analyzer.result(0).reliable);
}

void test_frequency_accepts_quantization_jitter_and_incremental_results() {
    I2cFrequencyAnalyzer analyzer;
    for (unsigned i = 0; i < 12; ++i) {
        // Feed separate chunks without adding a false burst boundary.
        analyzer.addPulse(30, false);
        analyzer.result(20000000);
        analyzer.addPulse(19 + i % 3, true);
    }
    analyzer.addPulse(1, false);
    const auto result = analyzer.result(20000000);
    TEST_ASSERT_TRUE(result.reliable);
    TEST_ASSERT_EQUAL_UINT32(12, result.acceptedCycles);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 400000, result.frequencyHz);
}

} // namespace i2c_frequency_analyzer_tests

void runI2cFrequencyAnalyzerTests() {
    using namespace i2c_frequency_analyzer_tests;
    RUN_TEST(test_frequency_measures_standard_fast_and_fast_plus_clocks);
    RUN_TEST(test_frequency_excludes_idle_gaps_stretching_and_short_glitches);
    RUN_TEST(test_frequency_does_not_join_bursts_or_use_truncated_tails);
    RUN_TEST(test_frequency_rejects_idle_bus_insufficient_cycles_and_mixed_speeds);
    RUN_TEST(test_frequency_accepts_quantization_jitter_and_incremental_results);
}
