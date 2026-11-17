#include <unity.h>

#include "Vendors/MakeHex.h"
#include <numeric>

namespace makehex_tests {

std::vector<float> encode(const char* name, int device, int subdevice, int function,
                          int& hz) {
    return encodeRemoteCommand(InfraredCommand(_RAW, device, subdevice, function), name, hz);
}

uint64_t pulseDistancePayload(const std::vector<float>& raw, unsigned bits,
                              float mark, float zero, float one) {
    uint64_t payload = 0;
    for (unsigned bit = 0; bit < bits; ++bit) {
        TEST_ASSERT_EQUAL_FLOAT(mark, raw[2 + bit * 2]);
        const auto space = raw[3 + bit * 2];
        TEST_ASSERT_TRUE(space == zero || space == one);
        if (space == one) payload |= uint64_t(1) << bit;
    }
    return payload;
}

void test_added_definitions_are_reachable_and_generate_distinct_frames() {
    const char* names[] = {
        "akai", "bose", "canalsat", "directv", "grundig16", "grundig16_30", "gxb",
        "kathrein", "konka", "logitech", "nec1_f16", "nec1_rnc", "nec1_y1",
        "nec1_y2", "nec1_y3", "nec2_f16", "nrc16", "nrc17", "pacemss", "replay",
        "scatl_6", "sejin_1_38", "sejin_1_56", "velleman"
    };
    int hz = 0;
    const auto nec = encode("nec2", 1, 2, 21, hz);
    for (const char* name : names) {
        const auto protocol = InfraredProtocolMapper::toEnum(name);
        TEST_ASSERT_NOT_EQUAL(_IR_UNKNOWN, protocol);
        TEST_ASSERT_EQUAL_STRING(name, InfraredProtocolMapper::toString(protocol).c_str());
        const auto raw = encode(name, 1, 2, 21, hz);
        TEST_ASSERT_FALSE_MESSAGE(raw.empty(), name);
        TEST_ASSERT_EQUAL_UINT(0, raw.size() % 2);
        TEST_ASSERT_TRUE(hz >= 30000 && hz <= 60000);
        TEST_ASSERT_TRUE_MESSAGE(raw != nec, name);
        for (float duration : raw) TEST_ASSERT_TRUE(std::isfinite(duration) && duration > 0);
    }
}

void test_nec_default_subdevice_and_full_function_word() {
    int hz = 0;
    const auto raw = encode("nec2_f16", 0x34, -1, 0x1234, hz);
    TEST_ASSERT_EQUAL_UINT(68, raw.size());
    TEST_ASSERT_EQUAL_INT(38400, hz);
    TEST_ASSERT_EQUAL_HEX32(0x1234CB34, pulseDistancePayload(raw, 32, 564, 564, 1692));
    TEST_ASSERT_TRUE(raw == encode("nec2_f16", 0x34, 0xCB, 0x1234, hz));
}

void test_apple_payload_uses_pair_id_function_and_parity() {
    int hz = 0;
    const auto raw = encode("apple", 238, 0, 5, hz);
    TEST_ASSERT_EQUAL_INT(38400, hz);
    TEST_ASSERT_EQUAL_UINT(68, raw.size());
    TEST_ASSERT_EQUAL_HEX32(0x000B87EE, pulseDistancePayload(raw, 32, 564, 564, 1692));
    TEST_ASSERT_EQUAL_FLOAT(108000, std::accumulate(raw.begin(), raw.end(), 0.f));
    const auto paired = encode("apple", 238, 1, 5, hz);
    TEST_ASSERT_EQUAL_HEX32(0x010A87EE, pulseDistancePayload(paired, 32, 564, 564, 1692));
    TEST_ASSERT_TRUE(encode("apple", 238, -1, 5, hz).empty());
}

void test_nec42_preserves_thirteen_bit_address_and_extended_payload() {
    int hz = 0;
    const auto raw = encode("NEC42", 0x1234, -1, 0xA5, hz);
    TEST_ASSERT_EQUAL_UINT(88, raw.size());
    TEST_ASSERT_EQUAL_INT(38000, hz);
    const uint64_t expected = 0x1234ULL | (0xDCBULL << 13) |
                              (0xA5ULL << 26) | (0x5AULL << 34);
    TEST_ASSERT_EQUAL_HEX64(expected, pulseDistancePayload(raw, 42, 560, 560, 1690));
    TEST_ASSERT_EQUAL_FLOAT(145000, std::accumulate(raw.begin(), raw.end(), 0.f));
    const auto extended = encode("NEC42ext", 0x1234, 0x567, 0x1234, hz);
    TEST_ASSERT_EQUAL_UINT(88, extended.size());
    TEST_ASSERT_EQUAL_HEX64(0x1234ULL | (0x567ULL << 13) | (0x1234ULL << 26),
                            pulseDistancePayload(extended, 42, 560, 560, 1690));
    TEST_ASSERT_TRUE(encode("NEC42", 8192, -1, 1, hz).empty());
}

void test_samsung32_and_samsung20_have_different_payload_widths() {
    int hz = 0;
    const auto raw = encode("Samsung32", 7, -1, 2, hz);
    TEST_ASSERT_EQUAL_UINT(68, raw.size());
    TEST_ASSERT_EQUAL_INT(38000, hz);
    TEST_ASSERT_EQUAL_HEX32(0xFD020707, pulseDistancePayload(raw, 32, 550, 550, 1650));
    TEST_ASSERT_EQUAL_UINT(44, encode("Samsung20", 7, -1, 2, hz).size());
    TEST_ASSERT_EQUAL_UINT(116, encode("panasonic2", 1, 1, 30, hz).size());
}

void test_sirc_variants_repeat_three_frames_with_45ms_period() {
    const char* names[] = {"SIRC", "SIRC15", "SIRC20"};
    const unsigned widths[] = {12, 15, 20};
    int hz = 0;
    for (unsigned n = 0; n < 3; ++n) {
        const auto raw = encode(names[n], 1, 0x5A, 21, hz);
        const unsigned frameSize = 2 + 2 * widths[n];
        TEST_ASSERT_EQUAL_UINT(3 * frameSize, raw.size());
        TEST_ASSERT_EQUAL_INT(40000, hz);
        for (unsigned frame = 0; frame < 3; ++frame) {
            const auto begin = raw.begin() + frame * frameSize;
            TEST_ASSERT_EQUAL_FLOAT(45000, std::accumulate(begin, begin + frameSize, 0.f));
            TEST_ASSERT_EQUAL_FLOAT(2400, *begin);
            uint32_t payload = 0;
            for (unsigned b = 0; b < widths[n]; ++b) {
                const float mark = *(begin + 2 + 2 * b);
                TEST_ASSERT_TRUE(mark == 600 || mark == 1200);
                if (mark == 1200) payload |= 1u << b;
            }
            TEST_ASSERT_EQUAL_HEX32(n == 2 ? 0x5A095 : 0x095, payload);
        }
    }
}

void test_zenith_nested_symbols_and_long_leadout() {
    int hz = 0;
    const auto raw = encode("zenith", 5, 1, 10, hz);
    TEST_ASSERT_EQUAL_INT(40000, hz);
    TEST_ASSERT_EQUAL_UINT(34, raw.size());
    const float prefix[] = {520, 520, 520, 4160, 520, 5200, 520, 520, 520, 4160};
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(prefix, raw.data(), 10);
    TEST_ASSERT_EQUAL_FLOAT(94160, raw.back());
    TEST_ASSERT_TRUE(encode("zenith", 1, 0, 7, hz).empty());
}

void test_unknown_protocols_and_invalid_rc6_modes_are_rejected() {
    int hz = 0;
    for (const char* name : {"does-not-exist", "barco", "zaptor_56", "raw", "",
                             "RC6-8-20", "RC6-6-16", "RC6-6-49", "RC6-6-20junk"}) {
        TEST_ASSERT_TRUE_MESSAGE(encode(name, 1, 2, 3, hz).empty(), name);
    }
    TEST_ASSERT_TRUE(encode(nullptr, 1, 2, 3, hz).empty());
    TEST_ASSERT_FALSE(encode("RC6-6-20", 1, 2, 3, hz).empty());
    TEST_ASSERT_TRUE(encode("NEC", 1, -1, 3, hz) == encode("nec2", 1, -1, 3, hz));
    TEST_ASSERT_TRUE(encode("NECX", 1, -1, 3, hz) == encode("NECx2", 1, -1, 3, hz));
}

void test_irp_can_generate_more_than_256_durations() {
    char definition[] = "Frequency=38000\nZero=500,-500\nOne=500,-1500\n"
                        "Form=F:32,F:32,F:32,F:32,F:32,500,-40000\n";
    IRP irp;
    TEST_ASSERT_TRUE(irp.readIrpString(definition));
    irp.m_value['F' - 'A'] = 0x12345678;
    int single = 0, repeat = 0;
    std::vector<float> raw;
    irp.generate(&single, &repeat, raw);
    TEST_ASSERT_EQUAL_UINT(322, raw.size());
    TEST_ASSERT_EQUAL_INT(161, single + repeat);
    TEST_ASSERT_EQUAL_FLOAT(40000, raw.back());
}

} // namespace makehex_tests

void runMakeHexTests() {
    using namespace makehex_tests;
    RUN_TEST(test_added_definitions_are_reachable_and_generate_distinct_frames);
    RUN_TEST(test_nec_default_subdevice_and_full_function_word);
    RUN_TEST(test_apple_payload_uses_pair_id_function_and_parity);
    RUN_TEST(test_nec42_preserves_thirteen_bit_address_and_extended_payload);
    RUN_TEST(test_samsung32_and_samsung20_have_different_payload_widths);
    RUN_TEST(test_sirc_variants_repeat_three_frames_with_45ms_period);
    RUN_TEST(test_zenith_nested_symbols_and_long_leadout);
    RUN_TEST(test_unknown_protocols_and_invalid_rc6_modes_are_rejected);
    RUN_TEST(test_irp_can_generate_more_than_256_durations);
}
