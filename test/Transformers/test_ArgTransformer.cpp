#include <unity.h>

#include <array>
#include <string>
#include <vector>

#include "Transformers/ArgTransformer.h"

namespace arg_transformer_tests {

ArgTransformer transformer;

void test_hostname_validation() {
    TEST_ASSERT_TRUE(transformer.isValidHostname("Bit-Pirate-01"));
    TEST_ASSERT_TRUE(transformer.isValidHostname("a"));
    TEST_ASSERT_TRUE(transformer.isValidHostname(std::string(31, 'a')));
    for (const auto& name : std::vector<std::string>{"", "-pirate", "pirate-", "a_b", "a.b", "a b", "a\tb", std::string(32, 'a'), std::string("a\0b", 3), "caf\xC3\xA9"}) {
        TEST_ASSERT_FALSE_MESSAGE(transformer.isValidHostname(name), name.c_str());
    }
}

void test_parse_byte_list_parses_decimal_and_hex_values() {
    const auto actual = transformer.parseByteList("10 0x2A 64");
    const uint8_t expected[] = {10, 42, 64};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual.data(), 3);
}

void test_parse_byte_list_accepts_all_spellings_of_255() {
    const auto actual = transformer.parseByteList("255 0xFF 0xff");
    const uint8_t expected[] = {255, 255, 255};
    TEST_ASSERT_EQUAL_UINT32(3, actual.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual.data(), 3);
}

void test_parse_byte_list_returns_empty_vector_for_empty_input() {
    TEST_ASSERT_TRUE(transformer.parseByteList("").empty());
}

void test_parse_byte_list_ignores_invalid_partial_and_out_of_range_values() {
    const auto actual = transformer.parseByteList("12 invalid 1x -1 256 0xGG 34");
    const uint8_t expected[] = {12, 34};
    TEST_ASSERT_EQUAL_UINT32(2, actual.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual.data(), 2);
}

void test_parse_hex_list_is_strict_and_range_checked() {
    const auto actual = transformer.parseHexList("00 7f 0xA0 FF 100 1G");
    const uint8_t expected[] = {0x00, 0x7F, 0xA0, 0xFF};
    TEST_ASSERT_EQUAL_UINT32(4, actual.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual.data(), 4);
}

void test_parse_hex_list16_accepts_common_separators_and_rejects_overflow() {
    const auto actual = transformer.parseHexList16("1, ABCD; FFFF 10000 nope");
    const uint16_t expected[] = {0x0001, 0xABCD, 0xFFFF};
    TEST_ASSERT_EQUAL_UINT32(3, actual.size());
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual.data(), 3);
}

void test_parse_hex_or_decimal_supports_boundaries() {
    TEST_ASSERT_EQUAL_UINT8(255, transformer.parseHexOrDec("0xFF"));
    TEST_ASSERT_EQUAL_UINT8(42, transformer.parseHexOrDec("42"));
    TEST_ASSERT_EQUAL_UINT8(0, transformer.parseHexOrDec("256"));
    TEST_ASSERT_EQUAL_UINT8(0, transformer.parseHexOrDec("12x"));
}

void test_parse_hex_or_decimal32_rejects_values_above_uint32() {
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, transformer.parseHexOrDec32("4294967295"));
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, transformer.parseHexOrDec32("0xDEADBEEF"));
    TEST_ASSERT_EQUAL_UINT32(0, transformer.parseHexOrDec32("4294967296"));
    TEST_ASSERT_EQUAL_UINT32(0, transformer.parseHexOrDec32("0x"));
}

void test_parse_hex_or_decimal64_supports_max_and_rejects_overflow() {
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, transformer.parseHexOrDec64("18446744073709551615"));
    TEST_ASSERT_EQUAL_UINT64(0, transformer.parseHexOrDec64("18446744073709551616"));
    TEST_ASSERT_EQUAL_UINT64(0x123456789ABCDEF0ULL,
                             transformer.parseHexOrDec64("0x123456789ABCDEF0"));
}

void test_parse_hex_bytes_accepts_separators() {
    uint8_t actual[3] = {};
    const uint8_t expected[] = {0xAA, 0xBB, 0x01};
    TEST_ASSERT_TRUE(transformer.parseHexBytes("AA:bb-01", actual, 3));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, 3);
}

void test_parse_hex_bytes_rejects_wrong_length_and_null_output() {
    uint8_t output[2] = {};
    TEST_ASSERT_FALSE(transformer.parseHexBytes("AA", output, 2));
    TEST_ASSERT_FALSE(transformer.parseHexBytes("AABB", nullptr, 2));
    TEST_ASSERT_FALSE(transformer.parseHexBytes("AABB", output, 0));
}

void test_split_args_collapses_whitespace() {
    const auto args = transformer.splitArgs("  alpha\tbeta   gamma ");
    TEST_ASSERT_EQUAL_UINT32(3, args.size());
    TEST_ASSERT_EQUAL_STRING("alpha", args[0].c_str());
    TEST_ASSERT_EQUAL_STRING("beta", args[1].c_str());
    TEST_ASSERT_EQUAL_STRING("gamma", args[2].c_str());
}

void test_parse_int_supports_decimal_prefixed_and_suffixed_hex() {
    int value = 0;
    TEST_ASSERT_TRUE(transformer.parseInt("-42", value));
    TEST_ASSERT_EQUAL_INT(-42, value);
    TEST_ASSERT_TRUE(transformer.parseInt("0x2A", value));
    TEST_ASSERT_EQUAL_INT(42, value);
    TEST_ASSERT_TRUE(transformer.parseInt("2Ah", value));
    TEST_ASSERT_EQUAL_INT(42, value);
}

void test_parse_int_rejects_partial_and_native_int_overflow() {
    int value = 7;
    TEST_ASSERT_FALSE(transformer.parseInt("12x", value));
    TEST_ASSERT_FALSE(transformer.parseInt("999999999999", value));
    TEST_ASSERT_EQUAL_INT(7, value);
}

void test_number_validation_rejects_empty_prefix_and_signs() {
    TEST_ASSERT_TRUE(transformer.isValidNumber("123"));
    TEST_ASSERT_TRUE(transformer.isValidNumber("0xBEEF"));
    TEST_ASSERT_FALSE(transformer.isValidNumber("0x"));
    TEST_ASSERT_FALSE(transformer.isValidNumber("-1"));
    TEST_ASSERT_FALSE(transformer.isValidNumber("12.5"));
}

void test_float_validation_requires_complete_value() {
    TEST_ASSERT_TRUE(transformer.isValidFloat("-12.50"));
    TEST_ASSERT_TRUE(transformer.isValidFloat("1e3"));
    TEST_ASSERT_FALSE(transformer.isValidFloat("1.2f"));
    TEST_ASSERT_FALSE(transformer.isValidFloat(""));
}

void test_numeric_code_validation_checks_digits_and_swaps_limits() {
    TEST_ASSERT_TRUE(transformer.isValidNumericCode("0123", 6, 4));
    TEST_ASSERT_FALSE(transformer.isValidNumericCode("12A3", 4, 6));
    TEST_ASSERT_FALSE(transformer.isValidNumericCode("123", 4, 6));
}

void test_signed_number_validation_supports_signed_hex() {
    TEST_ASSERT_TRUE(transformer.isValidSignedNumber("-127"));
    TEST_ASSERT_TRUE(transformer.isValidSignedNumber("+0x7F"));
    TEST_ASSERT_FALSE(transformer.isValidSignedNumber("-0x"));
    TEST_ASSERT_FALSE(transformer.isValidSignedNumber("+"));
}

void test_unsigned_conversions_reject_sign_and_apply_documented_bounds() {
    TEST_ASSERT_EQUAL_UINT8(255, transformer.toUint8("999"));
    TEST_ASSERT_EQUAL_UINT8(0, transformer.toUint8("-1"));
    TEST_ASSERT_EQUAL_UINT8(42, transformer.toUint8("0x2A"));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, transformer.toUint32("0xFFFFFFFF"));
    TEST_ASSERT_EQUAL_UINT32(0, transformer.toUint32("-1"));
}

void test_signed_int8_conversion_clamps_to_symmetric_range() {
    TEST_ASSERT_EQUAL_INT8(127, transformer.toClampedInt8("999"));
    TEST_ASSERT_EQUAL_INT8(-127, transformer.toClampedInt8("-999"));
    TEST_ASSERT_EQUAL_INT8(42, transformer.toClampedInt8("0x2A"));
    TEST_ASSERT_EQUAL_INT8(0, transformer.toClampedInt8("bad"));
}

void test_string_cleanup_lowercases_and_preserves_allowed_controls() {
    TEST_ASSERT_EQUAL_STRING("hello-42", transformer.toLower("HeLLo-42").c_str());
    const std::string dirty({'A', '\x01', '\n', 'B', '\t'});
    TEST_ASSERT_EQUAL_STRING("A\nB\t", transformer.filterPrintable(dirty).c_str());
}

void test_decode_escapes_handles_controls_hex_and_unknown_sequences() {
    const std::string decoded = transformer.decodeEscapes("A\\nB\\x21\\q\\\\");
    TEST_ASSERT_EQUAL_STRING("A\nB!\\q\\", decoded.c_str());
    TEST_ASSERT_EQUAL_STRING("abc\\x", transformer.decodeEscapes("abc\\x").c_str());
}

void test_numeric_formatting_produces_stable_text() {
    TEST_ASSERT_EQUAL_STRING("00AF", transformer.toHex(0xAF, 4).c_str());
    TEST_ASSERT_EQUAL_STRING("3.142", transformer.formatFloat(3.14159, 3).c_str());
    TEST_ASSERT_EQUAL_STRING("-2.50", transformer.toFixed2(-2.5f).c_str());
}

void test_ascii_line_contains_address_hex_and_printable_projection() {
    const std::vector<uint8_t> data = {'A', 0x00, 'Z'};
    const std::string line = transformer.toAsciiLine(0x12, data);
    TEST_ASSERT_TRUE(line.find("000012: 41 00 5A") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("A.Z") != std::string::npos);
}

void test_hex_ascii_formats_rows_and_handles_invalid_row_width() {
    const uint8_t data[] = {'A', 0x00, '~', ' ', 'B'};
    const std::string formatted = transformer.formatHexAscii(data, 5, true, 4);
    TEST_ASSERT_TRUE(formatted.find("41 00 7E 20  | A.~ ") != std::string::npos);
    TEST_ASSERT_TRUE(formatted.find("42           | B") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", transformer.formatHexAscii(data, 5, true, 0).c_str());
}

void test_binary_and_ascii_views_use_significant_bytes() {
    TEST_ASSERT_EQUAL_STRING("00000001 00000010", transformer.toBinString(0x0102).c_str());
    TEST_ASSERT_EQUAL_STRING("AB.!", transformer.toAsciiString(0x41420021).c_str());
    TEST_ASSERT_EQUAL_STRING("", transformer.toAsciiString(0x3132).c_str());
}

void test_parse_mac_accepts_colon_dash_and_compact_forms() {
    std::array<uint8_t, 6> actual{};
    const uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_TRUE(transformer.parseMac("AA:BB:CC:DD:EE:FF", actual));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual.data(), 6);
    TEST_ASSERT_TRUE(transformer.parseMac("AA-BB-CC-DD-EE-FF", actual));
    TEST_ASSERT_TRUE(transformer.parseMac("AABBCCDDEEFF", actual));
    TEST_ASSERT_FALSE(transformer.parseMac("AA:BB:CC:DD:EE", actual));
}

void test_url_helpers_add_scheme_and_extract_host() {
    TEST_ASSERT_EQUAL_STRING("https://example.com/path",
                             transformer.ensureHttpScheme("example.com/path").c_str());
    TEST_ASSERT_EQUAL_STRING("http://example.com",
                             transformer.ensureHttpScheme("http://example.com").c_str());
    TEST_ASSERT_EQUAL_STRING("example.com:8080",
                             transformer.extractHostFromUrl("https://example.com:8080/a").c_str());
}

void test_normalize_lines_converts_lf_without_duplicating_existing_crlf() {
    TEST_ASSERT_EQUAL_STRING("a\r\nb\r\nc",
                             transformer.normalizeLines("a\nb\r\nc").c_str());
}

void test_parse_binary_list_supports_compact_and_spaced_forms_strictly() {
    const uint8_t expected[] = {1, 0, 1, 1};
    auto compact = transformer.parse01List("1011");
    auto spaced = transformer.parse01List("1 0 1 1");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, compact.data(), 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, spaced.data(), 4);
    TEST_ASSERT_TRUE(transformer.parse01List("10x1").empty());
    TEST_ASSERT_TRUE(transformer.parse01List("1 0 2").empty());
}

void test_lsb_pack_and_unpack_roundtrip_non_byte_aligned_bits() {
    const std::vector<uint8_t> bits = {1, 0, 1, 0, 0, 0, 0, 1, 1};
    const auto packed = transformer.packLsbFirst(bits);
    const uint8_t expectedPacked[] = {0x85, 0x01};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedPacked, packed.data(), 2);

    std::vector<uint8_t> unpacked;
    TEST_ASSERT_TRUE(transformer.unpackLsbFirst(packed, bits.size(), unpacked));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bits.data(), unpacked.data(), bits.size());
}

void test_lsb_unpack_rejects_short_input_without_resizing_output() {
    std::vector<uint8_t> output = {9};
    TEST_ASSERT_FALSE(transformer.unpackLsbFirst({0xFF}, 9, output));
    TEST_ASSERT_EQUAL_UINT32(1, output.size());
    TEST_ASSERT_EQUAL_UINT8(9, output[0]);
}

void test_pattern_parser_decodes_text_pattern() {
    std::string text;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;
    bool isHex = true;
    TEST_ASSERT_TRUE(transformer.parsePattern("hello\\n", text, bytes, mask, isHex));
    TEST_ASSERT_FALSE(isHex);
    TEST_ASSERT_EQUAL_STRING("hello\n", text.c_str());
    TEST_ASSERT_TRUE(bytes.empty());
}

void test_pattern_parser_supports_hex_wildcards_and_prefixes() {
    std::string text;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;
    bool isHex = false;
    TEST_ASSERT_TRUE(transformer.parsePattern("hex{AA ?? 0x1f}", text, bytes, mask, isHex));
    const uint8_t expectedBytes[] = {0xAA, 0x00, 0x1F};
    const uint8_t expectedMask[] = {1, 0, 1};
    TEST_ASSERT_TRUE(isHex);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBytes, bytes.data(), 3);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedMask, mask.data(), 3);
}

void test_pattern_parser_rejects_empty_and_malformed_hex_patterns() {
    std::string text;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;
    bool isHex = false;
    TEST_ASSERT_FALSE(transformer.parsePattern("", text, bytes, mask, isHex));
    TEST_ASSERT_FALSE(transformer.parsePattern("hex{GG}", text, bytes, mask, isHex));
    TEST_ASSERT_FALSE(transformer.parsePattern("hex{}", text, bytes, mask, isHex));
}

}  // namespace arg_transformer_tests

void runArgTransformerTests() {
    using namespace arg_transformer_tests;
    RUN_TEST(test_hostname_validation);
    RUN_TEST(test_parse_byte_list_parses_decimal_and_hex_values);
    RUN_TEST(test_parse_byte_list_accepts_all_spellings_of_255);
    RUN_TEST(test_parse_byte_list_returns_empty_vector_for_empty_input);
    RUN_TEST(test_parse_byte_list_ignores_invalid_partial_and_out_of_range_values);
    RUN_TEST(test_parse_hex_list_is_strict_and_range_checked);
    RUN_TEST(test_parse_hex_list16_accepts_common_separators_and_rejects_overflow);
    RUN_TEST(test_parse_hex_or_decimal_supports_boundaries);
    RUN_TEST(test_parse_hex_or_decimal32_rejects_values_above_uint32);
    RUN_TEST(test_parse_hex_or_decimal64_supports_max_and_rejects_overflow);
    RUN_TEST(test_parse_hex_bytes_accepts_separators);
    RUN_TEST(test_parse_hex_bytes_rejects_wrong_length_and_null_output);
    RUN_TEST(test_split_args_collapses_whitespace);
    RUN_TEST(test_parse_int_supports_decimal_prefixed_and_suffixed_hex);
    RUN_TEST(test_parse_int_rejects_partial_and_native_int_overflow);
    RUN_TEST(test_number_validation_rejects_empty_prefix_and_signs);
    RUN_TEST(test_float_validation_requires_complete_value);
    RUN_TEST(test_numeric_code_validation_checks_digits_and_swaps_limits);
    RUN_TEST(test_signed_number_validation_supports_signed_hex);
    RUN_TEST(test_unsigned_conversions_reject_sign_and_apply_documented_bounds);
    RUN_TEST(test_signed_int8_conversion_clamps_to_symmetric_range);
    RUN_TEST(test_string_cleanup_lowercases_and_preserves_allowed_controls);
    RUN_TEST(test_decode_escapes_handles_controls_hex_and_unknown_sequences);
    RUN_TEST(test_numeric_formatting_produces_stable_text);
    RUN_TEST(test_ascii_line_contains_address_hex_and_printable_projection);
    RUN_TEST(test_hex_ascii_formats_rows_and_handles_invalid_row_width);
    RUN_TEST(test_binary_and_ascii_views_use_significant_bytes);
    RUN_TEST(test_parse_mac_accepts_colon_dash_and_compact_forms);
    RUN_TEST(test_url_helpers_add_scheme_and_extract_host);
    RUN_TEST(test_normalize_lines_converts_lf_without_duplicating_existing_crlf);
    RUN_TEST(test_parse_binary_list_supports_compact_and_spaced_forms_strictly);
    RUN_TEST(test_lsb_pack_and_unpack_roundtrip_non_byte_aligned_bits);
    RUN_TEST(test_lsb_unpack_rejects_short_input_without_resizing_output);
    RUN_TEST(test_pattern_parser_decodes_text_pattern);
    RUN_TEST(test_pattern_parser_supports_hex_wildcards_and_prefixes);
    RUN_TEST(test_pattern_parser_rejects_empty_and_malformed_hex_patterns);
}
