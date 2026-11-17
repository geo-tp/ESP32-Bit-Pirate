#include <unity.h>

#include "Transformers/InfraredRemoteTransformer.h"
#include "Vendors/MakeHex.h"
#include <numeric>
#include <cmath>

namespace infrared_remote_transformer_tests {

void test_valid_file_requires_ir_filetype_on_first_line() {
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::isValidInfraredFile("Filetype: IR\nVersion: 1\n"));
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::isValidInfraredFile("Version: 1\nFiletype: IR\n"));
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::isValidInfraredFile(""));
}

void test_parsed_command_imports_little_endian_address_and_one_byte_command() {
    const std::string file =
        "Filetype: IR\n"
        "Version: 1\n\n"
        "name: Power\n"
        "type: parsed\n"
        "protocol: NECext\n"
        "address: 34 12\n"
        "command: AB\n";

    auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);

    TEST_ASSERT_EQUAL_UINT32(1, commands.size());
    TEST_ASSERT_EQUAL_STRING("Power", commands[0].functionName.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NECext), static_cast<int>(commands[0].protocol));
    TEST_ASSERT_EQUAL_HEX16(0x1234, commands[0].address);
    TEST_ASSERT_EQUAL_HEX8(0xAB, commands[0].function);
    TEST_ASSERT_TRUE(commands[0].error.empty());
}

void test_raw_command_imports_frequency_duty_and_data() {
    const std::string file =
        "Filetype: IR\n"
        "Version: 1\n\n"
        "name: RawOne\n"
        "type: raw\n"
        "frequency: 38000\n"
        "duty_cycle: 0.33\n"
        "data: 9000 4500 560\n";

    auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);

    TEST_ASSERT_EQUAL_UINT32(1, commands.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(_RAW), static_cast<int>(commands[0].protocol));
    TEST_ASSERT_EQUAL_INT(38000, commands[0].frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.33f, commands[0].dutyCycle);
    TEST_ASSERT_EQUAL_UINT32(3, commands[0].rawData.size());
    TEST_ASSERT_EQUAL_UINT16(9000, commands[0].rawData[0]);
    TEST_ASSERT_EQUAL_UINT16(4500, commands[0].rawData[1]);
    TEST_ASSERT_EQUAL_UINT16(560, commands[0].rawData[2]);


}

void test_transform_to_file_format_writes_parsed_command_with_expected_byte_widths() {
    InfraredFileRemoteCommand cmd{};
    cmd.functionName = "Power";
    cmd.protocol = NECext;
    cmd.fileProtocol = "NECext";
    cmd.address = 0x1234;
    cmd.function = 0xAB;

    const std::string file = InfraredRemoteTransformer::transformToFileFormat("remote.ir", {cmd});

    TEST_ASSERT_TRUE(file.find("Filetype: IR") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("name: Power") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("address: 34 12") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("command: AB 00 00 00\n") != std::string::npos);

}

void test_transform_to_file_format_writes_raw_command() {

    InfraredFileRemoteCommand cmd{};
    cmd.functionName = "RawOne";
    cmd.protocol = _RAW;
    cmd.frequency = 38000;
    cmd.dutyCycle = 0.33f;
    cmd.rawData = {9000, 4500, 560};

    const std::string file = InfraredRemoteTransformer::transformToFileFormat("remote.ir", {cmd});

    TEST_ASSERT_TRUE(file.find("type: raw") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("frequency: 38000") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("duty_cycle: 0.33") != std::string::npos);
    TEST_ASSERT_TRUE(file.find("data: 9000 4500 560") != std::string::npos);
}

void test_extract_function_names_preserves_order() {
    InfraredFileRemoteCommand first{};
    first.functionName = "Power";
    InfraredFileRemoteCommand second{};
    second.functionName = "VolumeUp";

    const auto names = InfraredRemoteTransformer::extractFunctionNames({first, second});

    TEST_ASSERT_EQUAL_UINT32(2, names.size());
    TEST_ASSERT_EQUAL_STRING("Power", names[0].c_str());
    TEST_ASSERT_EQUAL_STRING("VolumeUp", names[1].c_str());
}


void test_flipper_fields_round_trip_without_narrowing_or_protocol_aliasing() {
    const std::string file =
        "Filetype: IR signals file\nVersion: 1\n"
        "name: Extended\ntype: parsed\nprotocol: NEC42ext\n"
        "address: 34 12 CD 02\ncommand: FE DC 00 00\n";
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);
    TEST_ASSERT_EQUAL_UINT(1, commands.size());
    TEST_ASSERT_TRUE(commands[0].error.empty());
    TEST_ASSERT_EQUAL_HEX32(0x02CD1234, commands[0].address);
    TEST_ASSERT_EQUAL_HEX32(0xDCFE, commands[0].function);
    const auto exported = InfraredRemoteTransformer::transformToFileFormat("test", commands);
    const auto imported = InfraredRemoteTransformer::transformFromFileFormat(exported);
    TEST_ASSERT_TRUE(commands == imported);
    TEST_ASSERT_TRUE(exported.find("protocol: NEC42ext") != std::string::npos);
}

void test_raw_round_trip_preserves_long_spaces_and_exact_carrier() {
    const std::string file =
        "\xef\xbb\xbf# Remote\r\nFiletype: IR signals file\r\nVersion: 1\r\n"
        "name: Raw\ntype: raw\nfrequency: 38400\nduty_cycle: 0.4\n"
        "data: 520 94160 520 150000\n";
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);
    TEST_ASSERT_EQUAL_UINT(1, commands.size());
    TEST_ASSERT_TRUE(commands[0].error.empty());
    TEST_ASSERT_EQUAL_UINT32(94160, commands[0].rawData[1]);
    const auto copy = commands;
    TEST_ASSERT_TRUE(copy == InfraredRemoteTransformer::transformFromFileFormat(
        InfraredRemoteTransformer::transformToFileFormat("raw", commands)));
}

void test_legacy_bit_pirate_files_keep_their_dsf_layout() {
    const std::string file = "Filetype: IR\nVersion: 1\nname: Power\ntype: parsed\n"
        "protocol: nec1\naddress: 34 12\ncommand: AB\n";
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);
    TEST_ASSERT_EQUAL_UINT(1, commands.size());
    TEST_ASSERT_TRUE(commands[0].error.empty());
    TEST_ASSERT_TRUE(commands[0].legacyDsf);
    TEST_ASSERT_EQUAL_HEX16(0x1234, commands[0].address);
    TEST_ASSERT_TRUE(commands == InfraredRemoteTransformer::transformFromFileFormat(
        InfraredRemoteTransformer::transformToFileFormat("legacy", commands)));
}

void test_invalid_commands_are_reported_without_throwing_or_using_nec() {
    const std::string header = "Filetype: IR signals file\nVersion: 1\nname: Bad\n";
    const char* invalid[] = {
        "type: parsed\nprotocol: NEC\naddress: FF FF 00 00\ncommand: 01 00 00 00\n",
        "type: parsed\nprotocol: Mystery\naddress: 01\ncommand: 01\n",
        "type: parsed\nprotocol: NEC\naddress: GG\ncommand: 01\n",
        "type: parsed\nprotocol: NEC\naddress: 01 00 00 00 00\ncommand: 01\n",
        "type: parsed\nprotocol: NEC\naddress: 01\n",
        "type: raw\nfrequency: 38000junk\nduty_cycle: 0.33\ndata: 520 500\n",
        "type: raw\nfrequency: 38000\nduty_cycle: nan\ndata: 520 500\n",
        "type: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 520 -1\n",
        "type: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 520 4294967296\n",
        "type: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 520 0\n"
    };
    for (const auto* body : invalid) {
        const auto commands = InfraredRemoteTransformer::transformFromFileFormat(header + body);
        TEST_ASSERT_EQUAL_UINT(1, commands.size());
        TEST_ASSERT_FALSE(commands[0].error.empty());
        TEST_ASSERT_TRUE(commands[0].rawData.empty());
    }
}

void test_legacy_header_detection_matches_validation_with_bom_comments_and_crlf() {
    for (const char* prefix : {"", "\xef\xbb\xbf", "\xef\xbb\xbf# Legacy remote\r\n\r\n", " # comment\n"}) {
        const std::string file = std::string(prefix) +
            " Filetype: IR \r\nVersion: 1\r\nname: Power\r\ntype: parsed\r\n"
            "protocol: nec1\r\naddress: 34 12\r\ncommand: AB\r\n";
        TEST_ASSERT_TRUE(InfraredRemoteTransformer::isValidInfraredFile(file));
        const auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);
        TEST_ASSERT_EQUAL_UINT(1, commands.size());
        TEST_ASSERT_TRUE(commands[0].error.empty());
        TEST_ASSERT_TRUE(commands[0].legacyDsf);
        TEST_ASSERT_EQUAL_HEX32(0x1234, commands[0].address);
    }
}

void test_record_append_accepts_exact_limit_and_rejects_next_frame_atomically() {
    std::string text = "Filetype: IR signals file\nVersion: 1\n\n";
    const auto header = text;
    const std::vector<uint16_t> timings{65535, 65535, 560};
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawCommand(text, "power", timings, 38));
    const auto size = text.size();
    const auto expected = text;
    text = header;
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::appendRawCommand(text, "power", timings, 38, size - 1));
    TEST_ASSERT_TRUE(text == header);
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawCommand(text, "power", timings, 38, size));
    TEST_ASSERT_TRUE(text == expected);
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::appendRawCommand(text, "power", timings, 38, size));
    TEST_ASSERT_TRUE(text == expected);
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(text);
    TEST_ASSERT_EQUAL_UINT(1, commands.size());
    TEST_ASSERT_TRUE(commands[0].error.empty());
    TEST_ASSERT_EQUAL_UINT32(65535, commands[0].rawData[0]);
}

void test_record_append_bounds_complete_file_and_validates_frames() {
    const std::string header = "Filetype: IR signals file\nVersion: 1\n\n";
    std::string text = header;
    const std::vector<uint16_t> timings(511, 12000);
    size_t count = 0;
    while (InfraredRemoteTransformer::appendRawCommand(text, "power", timings, 38)) ++count;
    TEST_ASSERT_TRUE(count > 0 && count < 64);
    TEST_ASSERT_TRUE(text.size() <= 32 * 1024);
    TEST_ASSERT_TRUE(text.capacity() <= 32 * 1024);
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(text);
    TEST_ASSERT_EQUAL_UINT(count, commands.size());
    for (const auto& command : commands) TEST_ASSERT_TRUE(command.error.empty());
    for (const auto& invalid : std::vector<std::vector<uint16_t>>{{}, {0, 560}, std::vector<uint16_t>(200, 65535)}) {
        text = header;
        TEST_ASSERT_FALSE(InfraredRemoteTransformer::appendRawCommand(text, "power", invalid, 38));
        TEST_ASSERT_TRUE(text == header);
    }
}

}  // namespace infrared_remote_transformer_tests


// Waveform generation and RAW timing tests.
namespace infrared_remote_transformer_tests {
InfraredWaveform encode(const char* protocol, uint32_t address, uint32_t function, bool toggle = false) {
    InfraredFileRemoteCommand command;
    command.fileProtocol = protocol;
    command.address = address;
    command.function = function;
    return InfraredRemoteTransformer::fromFileCommand(command, toggle);
}

uint64_t pulsePayload(const InfraredWaveform& wave, unsigned bits, uint32_t unit) {
    TEST_ASSERT_EQUAL_UINT(3 + bits * 2, wave.pulses.size());
    uint64_t value = 0;
    for (unsigned i = 0; i < bits; ++i) {
        TEST_ASSERT_EQUAL_UINT32(unit, wave.pulses[2 + i * 2]);
        if (wave.pulses[3 + i * 2] > unit) value |= 1ULL << i;
    }
    return value;
}

void test_flipper_nec_extended_keeps_independent_function_bytes() {
    const auto standard = encode("NEC", 0x34, 0xA5);
    TEST_ASSERT_EQUAL_HEX32(0x5AA5CB34, pulsePayload(standard, 32, 560));
    const auto extended = encode("NECext", 0x1234, 0xDCFE);
    TEST_ASSERT_EQUAL_HEX32(0xDCFE1234, pulsePayload(extended, 32, 560));
    TEST_ASSERT_EQUAL_UINT32(38000, extended.frequencyHz);
    const auto samsung = encode("Samsung32", 7, 2);
    TEST_ASSERT_EQUAL_HEX32(0xFD020707, pulsePayload(samsung, 32, 550));
    TEST_ASSERT_EQUAL_UINT32(4500, samsung.pulses[0]);
}

void test_flipper_nec42_keeps_26_bit_address_and_full_unsigned_command() {
    const auto standard = encode("NEC42", 0x1234, 0xA5);
    TEST_ASSERT_EQUAL_HEX64(0x1234ULL | (0xDCBULL << 13) | (0xA5ULL << 26) | (0x5AULL << 34),
                          pulsePayload(standard, 42, 560));
    const auto extended = encode("NEC42ext", 0x2CD1234, 0xDCFE);
    TEST_ASSERT_EQUAL_HEX64(0x2CD1234ULL | (0xDCFEULL << 26), pulsePayload(extended, 42, 560));
}

void test_flipper_sony_uses_packed_address_and_three_frames() {
    const char* names[] = {"SIRC", "SIRC15", "SIRC20"};
    const unsigned widths[] = {12, 15, 20};
    const uint32_t addresses[] = {31, 255, 0x1ABC};
    for (unsigned n = 0; n < 3; ++n) {
        const auto wave = encode(names[n], addresses[n], 0x55);
        const unsigned frameSize = 2 + widths[n] * 2;
        TEST_ASSERT_EQUAL_UINT(frameSize * 3 - 1, wave.pulses.size());
        TEST_ASSERT_EQUAL_UINT32(40000, wave.frequencyHz);
        for (unsigned frame = 0; frame < 3; ++frame) {
            const auto begin = wave.pulses.begin() + frame * frameSize;
            TEST_ASSERT_EQUAL_UINT32(2400, *begin);
            if (frame < 2) TEST_ASSERT_EQUAL_UINT32(45000, std::accumulate(begin, begin + frameSize, 0u));
            uint32_t value = 0;
            for (unsigned bit = 0; bit < widths[n]; ++bit) {
                const auto mark = *(begin + 2 + bit * 2);
                TEST_ASSERT_TRUE(mark == 600 || mark == 1200);
                if (mark == 1200) value |= 1u << bit;
            }
            TEST_ASSERT_EQUAL_HEX32((addresses[n] << 7) | 0x55, value);
        }
    }
}

void test_flipper_rc5_and_rc6_toggle_and_manchester_fields() {
    for (const char* protocol : {"RC5", "RC5X", "RC6"}) {
        const bool rc6 = std::string(protocol) == "RC6";
        const uint32_t command = std::string(protocol) == "RC5X" ? 0x52 : 0x12;
        for (bool toggle : {false, true}) {
            const auto wave = encode(protocol, 5, command, toggle);
            TEST_ASSERT_EQUAL_UINT32(36000, wave.frequencyHz);
            std::vector<bool> halves;
            if (!rc6) halves.push_back(false); // RC5's leading idle half-bit.
            for (size_t i = rc6 ? 2 : 0; i < wave.pulses.size(); ++i) {
                const unsigned unit = rc6 ? 444 : 888;
                TEST_ASSERT_EQUAL_UINT(0, wave.pulses[i] % unit);
                for (unsigned n = 0; n < wave.pulses[i] / unit; ++n) halves.push_back(i % 2 == 0);
            }
            const size_t expectedHalves = rc6 ? 44 : 28;
            if (halves.size() + 1 == expectedHalves) halves.push_back(false);
            TEST_ASSERT_EQUAL_UINT(expectedHalves, halves.size());
            if (rc6) {
                TEST_ASSERT_EQUAL_UINT32(2666, wave.pulses[0]);
                TEST_ASSERT_EQUAL(toggle, halves[8]);
                TEST_ASSERT_EQUAL(toggle, halves[9]);
                TEST_ASSERT_EQUAL(!toggle, halves[10]);
                TEST_ASSERT_EQUAL(!toggle, halves[11]);
            } else {
                TEST_ASSERT_EQUAL(toggle, !halves[4]);
                TEST_ASSERT_EQUAL(command < 64, !halves[2]);
            }
            uint32_t value = 0;
            for (size_t i = rc6 ? 12 : 6; i < halves.size(); i += 2) {
                TEST_ASSERT_NOT_EQUAL(halves[i], halves[i + 1]);
                value = (value << 1) | (rc6 ? halves[i] : !halves[i]);
            }
            TEST_ASSERT_EQUAL_HEX32(rc6 ? (5 << 8) | command : (5 << 6) | (command & 63), value);
        }
    }
}

void test_flipper_rc5x_extension_comes_from_protocol_even_for_low_command_values() {
    const auto rc5x = encode("RC5X", 5, 0x12);
    TEST_ASSERT_TRUE(rc5x.pulses == encode("RC5X", 5, 0x52).pulses);
    TEST_ASSERT_TRUE(rc5x.pulses != encode("RC5", 5, 0x12).pulses);
    TEST_ASSERT_EQUAL_UINT32(1776, rc5x.pulses[0]); // First start mark joins inverted field bit.
}

void test_flipper_encoder_rejects_invalid_widths_and_unknown_names() {
    TEST_ASSERT_TRUE(encode("unknown", 0, 0).pulses.empty());
    TEST_ASSERT_TRUE(encode("NEC", 256, 1).pulses.empty());
    TEST_ASSERT_TRUE(encode("NECext", 0, 65536).pulses.empty());
    TEST_ASSERT_TRUE(encode("NEC42ext", 0x4000000, 0).pulses.empty());
    TEST_ASSERT_TRUE(encode("SIRC20", 8192, 0).pulses.empty());
    TEST_ASSERT_TRUE(encode("RC5", 0, 64).pulses.empty());
}

void test_capture_rejects_overflow_without_truncating_or_widening_buffers() {
    std::vector<uint16_t> timings;
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, false, 100000));
    TEST_ASSERT_TRUE(timings.empty());
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, true, 32767));
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, true, 32768));
    TEST_ASSERT_EQUAL_UINT16(65535, timings[0]);
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::appendRawTiming(timings, true, 1));
    TEST_ASSERT_TRUE(timings.empty());
    for (uint32_t duration : {65536u, 131070u, 0u, UINT32_MAX}) {
        timings = {560};
        TEST_ASSERT_FALSE(InfraredRemoteTransformer::appendRawTiming(timings, false, duration));
        TEST_ASSERT_TRUE(timings.empty());
    }
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, true, 560));
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, false, 65535));
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::appendRawTiming(timings, true, 560));
    TEST_ASSERT_EQUAL_UINT(3, timings.size());
}
}

// IR library decode normalization and Apple/NEC tests.
namespace infrared_remote_transformer_tests {

void test_apple_makehex_payload_decodes_to_original_command() {
    InfraredCommand decoded;
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::decodeApplePayload(0x000B87EEu, decoded));
    TEST_ASSERT_EQUAL_INT(_APPLE, decoded.getProtocol());
    TEST_ASSERT_EQUAL_INT(238, decoded.getDevice());
    TEST_ASSERT_EQUAL_INT(0, decoded.getSubdevice());
    TEST_ASSERT_EQUAL_INT(5, decoded.getFunction());
}

void test_apple_encoder_to_decoder_roundtrip() {
    InfraredCommand original(_APPLE, 238, 0, 5);
    int frequency = 0;
    auto waveform = encodeRemoteCommand(original, "apple", frequency);
    TEST_ASSERT_EQUAL_UINT(68, waveform.size());
    TEST_ASSERT_EQUAL_INT(38400, frequency);
    uint32_t payload = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        if (waveform[3 + bit * 2] > 1000.0f) payload |= (1u << bit);
    }
    InfraredCommand decoded;
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::decodeApplePayload(payload, decoded));
    TEST_ASSERT_EQUAL_INT(original.getDevice(), decoded.getDevice());
    TEST_ASSERT_EQUAL_INT(original.getSubdevice(), decoded.getSubdevice());
    TEST_ASSERT_EQUAL_INT(original.getFunction(), decoded.getFunction());
}

void test_apple_decodes_when_library_returns_msb_first_value() {
    InfraredCommand decoded;
    const uint32_t value = InfraredRemoteTransformer::reverseBits32(0x000B87EEu);
    TEST_ASSERT_TRUE(InfraredRemoteTransformer::decodeAppleEitherBitOrder(value, decoded));
    TEST_ASSERT_EQUAL_INT(5, decoded.getFunction());
}

void test_apple_rejects_invalid_parity_and_non_apple_nec() {
    InfraredCommand decoded;
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::decodeAppleEitherBitOrder(0x000A87EEu, decoded));
    TEST_ASSERT_FALSE(InfraredRemoteTransformer::decodeAppleEitherBitOrder(0x10EF04FBu, decoded));
    TEST_ASSERT_EQUAL_INT(_RAW, decoded.getProtocol());
}

void test_generic_native_decode_keeps_64_bits_without_narrowing() {
    InfraredCommand decoded;
    decoded.setProtocol(_IR_UNKNOWN);
    decoded.setNativeDecode("XMP", 64, 0xF123456789ABCDEFULL, true);
    TEST_ASSERT_EQUAL_STRING("XMP", decoded.getDisplayProtocolName().c_str());
    TEST_ASSERT_TRUE(decoded.getDecodedRawData() == 0xF123456789ABCDEFULL);
    TEST_ASSERT_EQUAL_UINT16(64, decoded.getDecodedBits());
    TEST_ASSERT_TRUE(decoded.isDecodedRepeat());
}

void test_extended_state_copy_is_bounded() {
    uint8_t bytes[130];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = i;
    InfraredCommand decoded;
    decoded.setProtocol(_IR_UNKNOWN);
    decoded.setNativeDecode("DAIKIN", 1040, 0);
    decoded.setDecodedState(bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL_UINT(InfraredCommand::MAX_DECODED_STATE_BYTES,
                           decoded.getDecodedState().size());
    TEST_ASSERT_TRUE(decoded.isDecodedStateTruncated());
    TEST_ASSERT_EQUAL_UINT8(95, decoded.getDecodedState().back());
}

} // namespace infrared_remote_transformer_tests

void runInfraredRemoteTransformerTests() {
    using namespace infrared_remote_transformer_tests;
    RUN_TEST(test_valid_file_requires_ir_filetype_on_first_line);
    RUN_TEST(test_parsed_command_imports_little_endian_address_and_one_byte_command);
    RUN_TEST(test_raw_command_imports_frequency_duty_and_data);
    RUN_TEST(test_transform_to_file_format_writes_parsed_command_with_expected_byte_widths);
    RUN_TEST(test_transform_to_file_format_writes_raw_command);
    RUN_TEST(test_extract_function_names_preserves_order);
    RUN_TEST(test_flipper_fields_round_trip_without_narrowing_or_protocol_aliasing);
    RUN_TEST(test_raw_round_trip_preserves_long_spaces_and_exact_carrier);
    RUN_TEST(test_invalid_commands_are_reported_without_throwing_or_using_nec);
    RUN_TEST(test_legacy_bit_pirate_files_keep_their_dsf_layout);
    RUN_TEST(test_legacy_header_detection_matches_validation_with_bom_comments_and_crlf);
    RUN_TEST(test_record_append_accepts_exact_limit_and_rejects_next_frame_atomically);
    RUN_TEST(test_record_append_bounds_complete_file_and_validates_frames);
    RUN_TEST(test_flipper_nec_extended_keeps_independent_function_bytes);
    RUN_TEST(test_flipper_nec42_keeps_26_bit_address_and_full_unsigned_command);
    RUN_TEST(test_flipper_sony_uses_packed_address_and_three_frames);
    RUN_TEST(test_flipper_rc5_and_rc6_toggle_and_manchester_fields);
    RUN_TEST(test_flipper_encoder_rejects_invalid_widths_and_unknown_names);
    RUN_TEST(test_capture_rejects_overflow_without_truncating_or_widening_buffers);
    RUN_TEST(test_flipper_rc5x_extension_comes_from_protocol_even_for_low_command_values);
    RUN_TEST(test_apple_makehex_payload_decodes_to_original_command);
    RUN_TEST(test_apple_decodes_when_library_returns_msb_first_value);
    RUN_TEST(test_apple_encoder_to_decoder_roundtrip);
    RUN_TEST(test_apple_rejects_invalid_parity_and_non_apple_nec);
    RUN_TEST(test_generic_native_decode_keeps_64_bits_without_narrowing);
    RUN_TEST(test_extended_state_copy_is_bounded);
}
