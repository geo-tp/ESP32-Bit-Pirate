#include <unity.h>

#include "Controllers/InfraredController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeI2cService.h"
#include "../Services/FakeInfraredService.h"
#include "../Services/FakeLittleFsService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeShell.h"
#include "../Views/FakeDeviceView.h"
#include "../Views/FakeTerminalView.h"

namespace infrared_controller_tests {

struct InfraredControllerFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeDeviceView device;
    FakeUtilityService utility;
    FakeInfraredService service;
    FakeLittleFsService littleFs;
    FakeI2cService i2c;
    ArgTransformer argTransformer;
    InfraredRemoteTransformer remoteTransformer;
    UserInputManager userInput{view, input, argTransformer};
    FakeShell remoteShell;
    HelpShell helpShell{view, input, userInput};
    InfraredController controller{
        view,
        input,
        device,
        utility,
        service,
        littleFs,
        i2c,
        argTransformer,
        remoteTransformer,
        userInput,
        remoteShell,
        helpShell
    };

    InfraredControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::Infrared);
        state.setTerminalMode(TerminalTypeEnum::Standalone);
        state.setInfraredTxPin(44);
        state.setInfraredRxPin(1);
        state.setInfraredProtocol(InfraredProtocolEnum::_NEC);
    }
};

void test_config_updates_selected_pins_and_configures_service() {
    InfraredControllerFixture fixture;
    fixture.input.queueLine("12");
    fixture.input.queueLine("13");

    fixture.controller.handleCommand(TerminalCommand("config"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(12, fixture.service.configurations[0].tx);
    TEST_ASSERT_EQUAL_UINT8(13, fixture.service.configurations[0].rx);
    TEST_ASSERT_EQUAL_UINT8(12, GlobalState::getInstance().getInfraredTxPin());
    TEST_ASSERT_EQUAL_UINT8(13, GlobalState::getInstance().getInfraredRxPin());
    TEST_ASSERT_TRUE(fixture.view.contains("Infrared configured"));
}

void test_ensure_configured_prompts_once_then_reapplies_saved_pins() {
    InfraredControllerFixture fixture;
    fixture.input.queueLine("14");
    fixture.input.queueLine("15");

    fixture.controller.ensureConfigured();
    GlobalState::getInstance().setInfraredTxPin(16);
    GlobalState::getInstance().setInfraredRxPin(17);
    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(2, fixture.service.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(14, fixture.service.configurations[0].tx);
    TEST_ASSERT_EQUAL_UINT8(15, fixture.service.configurations[0].rx);
    TEST_ASSERT_EQUAL_UINT8(16, fixture.service.configurations[1].tx);
    TEST_ASSERT_EQUAL_UINT8(17, fixture.service.configurations[1].rx);
}

void test_send_parses_device_subdevice_command_and_repeats_three_times() {
    InfraredControllerFixture fixture;
    GlobalState::getInstance().setInfraredProtocol(InfraredProtocolEnum::_RC5);

    fixture.controller.handleCommand(TerminalCommand("send", "1", "2 3"));

    TEST_ASSERT_EQUAL_UINT32(3, fixture.service.sentCommands.size());
    TEST_ASSERT_EQUAL(InfraredProtocolEnum::_RC5, fixture.service.sentCommands[0].getProtocol());
    TEST_ASSERT_EQUAL_INT(1, fixture.service.sentCommands[0].getDevice());
    TEST_ASSERT_EQUAL_INT(2, fixture.service.sentCommands[0].getSubdevice());
    TEST_ASSERT_EQUAL_INT(3, fixture.service.sentCommands[0].getFunction());
    TEST_ASSERT_EQUAL_UINT32(3, fixture.utility.sleepMsCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("IR command sent with protocol"));
}

void test_send_rejects_missing_or_invalid_numbers_without_transmitting() {
    InfraredControllerFixture missing;
    missing.controller.handleCommand(TerminalCommand("send", "1", "2"));

    InfraredControllerFixture invalid;
    invalid.controller.handleCommand(TerminalCommand("send", "AA", "2 3"));

    TEST_ASSERT_TRUE(missing.service.sentCommands.empty());
    TEST_ASSERT_TRUE(invalid.service.sentCommands.empty());
    TEST_ASSERT_TRUE(missing.view.contains("Missing argument"));
    TEST_ASSERT_TRUE(invalid.view.contains("Invalid number format"));
}

void test_receive_decoded_command_displays_fields_and_updates_device_view() {
    InfraredControllerFixture fixture;
    fixture.service.queueDecoded(InfraredProtocolEnum::_NEC, 12, 34, 56);
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("receive"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.startReceiverCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.stopReceiverCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Infrared signal received"));
    TEST_ASSERT_TRUE(fixture.view.contains("Device   : 12"));
    TEST_ASSERT_TRUE(fixture.view.contains("Command  : 56"));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.device.shownConfigs.size());
    TEST_ASSERT_EQUAL_STRING("RECV", fixture.device.shownConfigs[0].getMode().c_str());
}

void test_receive_native_long_protocol_shows_identity_and_state_without_fake_fields() {
    InfraredControllerFixture fixture;
    InfraredCommand decoded;
    decoded.setProtocol(_IR_UNKNOWN);
    decoded.setDevice(-1);
    decoded.setSubdevice(-1);
    decoded.setFunction(-1);
    decoded.setNativeDecode("DAIKIN", 72, 0);
    const uint8_t state[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    decoded.setDecodedState(state, sizeof(state));
    fixture.service.decodedQueue.push_back(decoded);
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("receive"));

    TEST_ASSERT_TRUE(fixture.view.contains("Protocol : DAIKIN"));
    TEST_ASSERT_TRUE(fixture.view.contains("Bits     : 72"));
    TEST_ASSERT_TRUE(fixture.view.contains("State    : 112233445566778899"));
    TEST_ASSERT_FALSE(fixture.view.contains("Device   : 0"));
}

void test_receive_raw_displays_timings_without_decoding() {
    InfraredControllerFixture fixture;
    fixture.input.queueLine("n");
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');
    fixture.service.queueRaw({9000, 4500, 560}, 38);

    fixture.controller.handleCommand(TerminalCommand("receive"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.startReceiverCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.stopReceiverCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("RAW Timings"));
    TEST_ASSERT_TRUE(fixture.view.contains("+9000 -4500 +560"));
}

void test_remote_command_delegates_to_shell() {
    InfraredControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("remote"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.remoteShell.runCalls);
}

void test_record_saves_selected_raw_command_to_littlefs() {
    InfraredControllerFixture fixture;
    fixture.service.queueRaw({9000, 4500, 560, 1690, 560}, 38);
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');
    fixture.input.queueLine("y");
    fixture.input.queueLine("power");
    fixture.input.queueLine("tv_remote");

    fixture.controller.handleCommand(TerminalCommand("record"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.startReceiverCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.stopReceiverCalls);
    TEST_ASSERT_EQUAL_STRING("/tv_remote.ir", fixture.littleFs.lastWritePath.c_str());
    TEST_ASSERT_TRUE(fixture.littleFs.lastWriteData.find("Filetype: IR") != std::string::npos);
    TEST_ASSERT_TRUE(fixture.littleFs.lastWriteData.find("name: power") != std::string::npos);
    TEST_ASSERT_TRUE(fixture.view.contains("INFRARED Record: Saved file"));
    const auto recorded = fixture.remoteTransformer.transformFromFileFormat(fixture.littleFs.lastWriteData);
    TEST_ASSERT_EQUAL_UINT(1, recorded.size());
    TEST_ASSERT_TRUE(recorded[0].error.empty());
    TEST_ASSERT_EQUAL_UINT(5, recorded[0].rawData.size());
    TEST_ASSERT_EQUAL_UINT32(38000, recorded[0].frequency);
}

void test_record_refuses_when_littlefs_has_not_enough_space() {
    InfraredControllerFixture fixture;
    fixture.littleFs.freeBytesValue = 1024;

    fixture.controller.handleCommand(TerminalCommand("record"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.service.startReceiverCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Not enough LittleFS space"));
}

void test_load_sends_selected_command_from_littlefs_file() {
    InfraredControllerFixture fixture;
    InfraredFileRemoteCommand command{};
    command.functionName = "power";
    command.protocol = InfraredProtocolEnum::_RC5;
    command.fileProtocol = "RC5";
    command.address = 4;
    command.function = 0x08;
    fixture.littleFs.files["/tv.ir"] = fixture.remoteTransformer.transformToFileFormat("tv", {command});
    fixture.input.queueLine("1");
    fixture.input.queueLine("1");
    fixture.input.queueLine("2");

    fixture.controller.handleCommand(TerminalCommand("load"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.sentFileCommands.size());
    TEST_ASSERT_EQUAL_STRING("power", fixture.service.sentFileCommands[0].functionName.c_str());
    TEST_ASSERT_EQUAL(InfraredProtocolEnum::_RC5, fixture.service.sentFileCommands[0].protocol);
    TEST_ASSERT_EQUAL_HEX16(4, fixture.service.sentFileCommands[0].address);
    TEST_ASSERT_EQUAL_HEX8(0x08, fixture.service.sentFileCommands[0].function);
    TEST_ASSERT_TRUE(fixture.view.contains("Sent command 'power'"));
}

void test_load_reports_missing_ir_files() {
    InfraredControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("load"));

    TEST_ASSERT_TRUE(fixture.view.contains("No .ir files found"));
}

void test_jam_selects_mode_carrier_and_density_then_stops() {
    InfraredControllerFixture fixture;
    fixture.input.queueLine("2");
    fixture.input.queueLine("");
    fixture.input.queueLine("7");
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("jam"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.sendJamCalls);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.service.lastJamMode);
    TEST_ASSERT_EQUAL_UINT16(38, fixture.service.lastJamKhz);
    TEST_ASSERT_EQUAL_UINT8(7, fixture.service.lastJamDensity);
    TEST_ASSERT_TRUE(fixture.view.contains("INFRARED Jam: Stopped by user"));
}

void test_unknown_command_displays_help() {
    InfraredControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("wat"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available INFRARED commands"));
}


void test_failed_send_does_not_report_success() {
    InfraredControllerFixture fixture;
    fixture.service.sendResult = false;
    fixture.controller.handleCommand(TerminalCommand("send", "1", "2 3"));
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.sentCommands.size());
    TEST_ASSERT_TRUE(fixture.view.contains("transmission failed"));
    TEST_ASSERT_FALSE(fixture.view.contains("IR command sent"));
}

void test_load_refuses_malformed_file_command() {
    InfraredControllerFixture fixture;
    fixture.littleFs.files["/bad.ir"] = "Filetype: IR signals file\nVersion: 1\n"
        "name: broken\ntype: parsed\nprotocol: unknown\naddress: 01\ncommand: 02\n";
    fixture.input.queueLine("1");
    fixture.input.queueLine("1");
    fixture.input.queueLine("2");
    fixture.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_TRUE(fixture.service.sentFileCommands.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Cannot send"));
    TEST_ASSERT_FALSE(fixture.view.contains("Sent command"));
}

void test_load_continues_after_mount_and_reports_mount_failure() {
    InfraredControllerFixture fixture;
    fixture.littleFs.mountedFlag = false;
    fixture.littleFs.files["/tv.ir"] = "Filetype: IR signals file\nVersion: 1\n"
        "name: Power\ntype: parsed\nprotocol: NEC\naddress: 04\ncommand: 08\n";
    fixture.input.queueLine("1");
    fixture.input.queueLine("1");
    fixture.input.queueLine("2");
    fixture.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_EQUAL_UINT(1, fixture.littleFs.beginCalls);
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.sentFileCommands.size());

    InfraredControllerFixture failed;
    failed.littleFs.mountedFlag = false;
    failed.littleFs.beginResult = false;
    failed.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_TRUE(failed.view.contains("LittleFS not mounted"));
    TEST_ASSERT_TRUE(failed.service.sentFileCommands.empty());
}

void test_record_stops_before_size_limit_and_file_can_be_loaded() {
    InfraredControllerFixture fixture;
    // Enough data to exceed 32 KiB well before the 64-command limit.
    for (unsigned i = 0; i < 64; ++i) {
        fixture.service.queueRaw(std::vector<uint16_t>(511, 12000), 38);
        fixture.service.queueRaw({560}, 38); // Frame discarded after the prompt.
        fixture.input.queueReadChar(KEY_NONE);
    }
    fixture.input.queueReadChar('\n');
    fixture.controller.handleCommand(TerminalCommand("record"));
    TEST_ASSERT_TRUE(fixture.view.contains("limit reached"));
    TEST_ASSERT_TRUE(fixture.view.contains("Saved file"));
    const auto& file = fixture.littleFs.lastWriteData;
    TEST_ASSERT_TRUE(file.size() <= 32 * 1024);
    const auto commands = InfraredRemoteTransformer::transformFromFileFormat(file);
    TEST_ASSERT_TRUE(!commands.empty() && commands.size() < 64);
    for (const auto& command : commands) TEST_ASSERT_TRUE(command.error.empty());
    fixture.input.queueLine("1");
    fixture.input.queueLine("1");
    fixture.input.queueLine(std::to_string(commands.size() + 1));
    fixture.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.sentFileCommands.size());
    TEST_ASSERT_TRUE(fixture.service.sentFileCommands[0] == commands[0]);
}

void test_load_accepts_32k_boundary_and_rejects_empty_or_larger_files() {
    const std::string content = "Filetype: IR signals file\nVersion: 1\n"
        "name: Power\ntype: parsed\nprotocol: NEC\naddress: 04\ncommand: 08\n#";
    for (size_t size : {size_t(0), size_t(32768), size_t(32769)}) {
        InfraredControllerFixture fixture;
        auto& file = fixture.littleFs.files["/tv.ir"];
        file = content;
        file.resize(size, '#');
        fixture.input.queueLine("1");
        fixture.input.queueLine("1");
        fixture.input.queueLine("2");
        fixture.controller.handleCommand(TerminalCommand("load"));
        TEST_ASSERT_EQUAL_UINT(size == 32768 ? 1 : 0, fixture.service.sentFileCommands.size());
        TEST_ASSERT_EQUAL(size != 32768, fixture.view.contains("empty or exceeds 32 KiB"));
    }
}

void test_replay_preserves_end_to_start_gaps_and_timer_wraparound() {
    for (uint32_t firstEnd : {100000u, UINT32_MAX - 100000u}) {
        InfraredControllerFixture fixture;
        fixture.utility.advanceTimeOnSleep = true;
        fixture.utility.useMicrosecondClock = true;
        fixture.service.queueRaw({10000, 20000, 10000}, 38); // 40 ms
        fixture.service.queueRaw({5000, 6000, 9000}, 38);   // 20 ms
        fixture.service.queueRaw({20000, 30000, 10000}, 38); // 60 ms
        unsigned received = 0;
        fixture.service.onReceiveRaw = [&]() {
            const uint32_t offsets[] = {0, 120500, 195750};
            fixture.utility.currentNowUs = firstEnd + offsets[received++];
        };
        std::vector<uint32_t> sentStarts;
        fixture.service.onSendRaw = [&](const std::vector<uint16_t>& timings) {
            sentStarts.push_back(fixture.utility.nowUs());
            for (auto timing : timings) fixture.utility.currentNowUs += timing;
        };
        for (unsigned i = 0; i < 3; ++i) fixture.input.queueReadChar(KEY_NONE);
        fixture.input.queueReadChar('\n');
        for (unsigned i = 0; i < 200; ++i) fixture.input.queueReadChar(KEY_NONE);
        fixture.controller.handleCommand(TerminalCommand("replay", "1"));
        TEST_ASSERT_EQUAL_UINT(3, sentStarts.size());
        TEST_ASSERT_EQUAL_UINT32(140500, sentStarts[1] - sentStarts[0]); // 40ms + 100.5ms silence
        TEST_ASSERT_EQUAL_UINT32(35250, sentStarts[2] - sentStarts[1]); // 20ms + 15.25ms silence
        TEST_ASSERT_TRUE(fixture.view.contains("Completed (1 loop(s))"));
    }
}

void test_replay_zero_gap_can_be_stopped_and_invalid_counts_do_not_start_rx() {
    InfraredControllerFixture fixture;
    fixture.service.queueRaw({560}, 38);
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n'); // Finish recording.
    fixture.input.queueReadChar(KEY_NONE); // Send first loop.
    fixture.input.queueReadChar('\n'); // Stop before second loop, even at zero gap.
    fixture.controller.handleCommand(TerminalCommand("replay"));
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.sentRawTimings.size());
    TEST_ASSERT_TRUE(fixture.view.contains("Stopped by user"));
    for (const char* count : {"-1", "abc", "4294967296", "0x100000000", "99999999999999999999999"}) {
        InfraredControllerFixture invalid;
        invalid.controller.handleCommand(TerminalCommand("replay", count));
        TEST_ASSERT_EQUAL_UINT(0, invalid.service.startReceiverCalls);
        TEST_ASSERT_TRUE(invalid.view.contains("Invalid replay count"));
    }
}

void test_replay_caps_total_capture_memory() {
    InfraredControllerFixture fixture;
    for (unsigned i = 0; i < 9; ++i) {
        fixture.service.queueRaw(std::vector<uint16_t>(512, 560), 38);
        fixture.input.queueReadChar(KEY_NONE);
    }
    for (unsigned i = 0; i < 8; ++i) fixture.input.queueReadChar(KEY_NONE);
    fixture.controller.handleCommand(TerminalCommand("replay", "1"));
    TEST_ASSERT_EQUAL_UINT(8, fixture.service.sentRawTimings.size());
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.stopReceiverCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("limit of 4096 timings"));
}

void test_send_rejects_narrowing_and_extra_arguments() {
    for (const char* device : {"-1", "32768", "65536"}) {
        InfraredControllerFixture fixture;
        fixture.controller.handleCommand(TerminalCommand("send", device, "-1 8"));
        TEST_ASSERT_TRUE(fixture.service.sentCommands.empty());
        TEST_ASSERT_TRUE(fixture.view.contains("Invalid range"));
    }
    InfraredControllerFixture extra;
    extra.controller.handleCommand(TerminalCommand("send", "4", "-1 8 9"));
    TEST_ASSERT_TRUE(extra.service.sentCommands.empty());
    TEST_ASSERT_TRUE(extra.view.contains("Too many arguments"));
}


void test_setprotocol_handles_large_numeric_input_and_valid_selection() {
    InfraredControllerFixture overflow;
    overflow.input.queueLine("99999999999999999999999999999999999999999");
    overflow.controller.handleCommand(TerminalCommand("setprotocol"));
    TEST_ASSERT_TRUE(overflow.view.contains("Invalid protocol number"));
    TEST_ASSERT_EQUAL(InfraredProtocolEnum::_NEC,
                      GlobalState::getInstance().getInfraredProtocol());

    InfraredControllerFixture valid;
    valid.input.queueLine("1");
    valid.controller.handleCommand(TerminalCommand("setprotocol"));
    TEST_ASSERT_TRUE(valid.view.contains("Protocol changed"));
}

void test_send_rejects_byte_truncation_but_allows_wide_makehex_protocols() {
    for (const auto proto : {InfraredProtocolEnum::_NEC,
                             InfraredProtocolEnum::_SAMSUNG,
                             InfraredProtocolEnum::_PANASONIC,
                             InfraredProtocolEnum::_PIONEER}) {
        for (const auto arg : {"300 -1 3", "4 300 3", "4 -1 300"}) {
            InfraredControllerFixture fixture;
            GlobalState::getInstance().setInfraredProtocol(proto);
            std::istringstream parts(arg);
            std::string device, rest;
            parts >> device;
            std::getline(parts, rest);
            fixture.controller.handleCommand(TerminalCommand("send", device, rest));
            TEST_ASSERT_TRUE(fixture.service.sentCommands.empty());
            TEST_ASSERT_TRUE(fixture.view.contains("field widths"));
        }
    }
    InfraredControllerFixture wide;
    GlobalState::getInstance().setInfraredProtocol(InfraredProtocolEnum::NEC42ext);
    wide.controller.handleCommand(TerminalCommand("send", "300", "300 300"));
    TEST_ASSERT_EQUAL_UINT32(3, wide.service.sentCommands.size());
}

void test_load_streams_one_command_without_readall() {
    InfraredControllerFixture fixture;
    auto& file = fixture.littleFs.files["/three.ir"];
    file = "\xef\xbb\xbf" "Filetype: IR signals file\r\nVersion: 1\r\n";
    file += "name: Raw\r\ntype: raw\r\nfrequency: 38000\r\nduty_cycle: 0.33\r\ndata:";
    for (unsigned i = 0; i < 1500; ++i) file += " 560";
    file += "\r\n# another command\r\n";
    file += "name: Power\r\ntype: parsed\r\nprotocol: NEC\r\naddress: 04\r\ncommand: 08\r\n";
    fixture.input.queueLine("1"); // file
    fixture.input.queueLine("2"); // Power; must not parse the large RAW frame
    fixture.input.queueLine("3"); // Exit file
    fixture.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_EQUAL_UINT(1, fixture.service.sentFileCommands.size());
    TEST_ASSERT_EQUAL_STRING("Power", fixture.service.sentFileCommands[0].functionName.c_str());
    TEST_ASSERT_EQUAL_UINT(0, fixture.littleFs.readAllCalls);
    TEST_ASSERT_EQUAL_UINT(2, fixture.littleFs.readChunksCalls);
}

void test_load_rejects_oversize_selected_command_without_consuming_all_ram() {
    InfraredControllerFixture fixture;
    auto& file = fixture.littleFs.files["/large.ir"];
    file = "Filetype: IR signals file\nVersion: 1\nname: Large\ntype: raw\n"
           "frequency: 38000\nduty_cycle: 0.33\ndata:";
    for (unsigned i = 0; i < 3200; ++i) file += " 560";
    fixture.input.queueLine("1");
    fixture.input.queueLine("1");
    fixture.input.queueLine("2");
    fixture.controller.handleCommand(TerminalCommand("load"));
    TEST_ASSERT_TRUE(fixture.service.sentFileCommands.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("8 KiB limit"));
    TEST_ASSERT_EQUAL_UINT(0, fixture.littleFs.readAllCalls);
}

}  // namespace infrared_controller_tests

void runInfraredControllerTests() {
    using namespace infrared_controller_tests;
    RUN_TEST(test_config_updates_selected_pins_and_configures_service);
    RUN_TEST(test_ensure_configured_prompts_once_then_reapplies_saved_pins);
    RUN_TEST(test_send_parses_device_subdevice_command_and_repeats_three_times);
    RUN_TEST(test_send_rejects_missing_or_invalid_numbers_without_transmitting);
    RUN_TEST(test_receive_decoded_command_displays_fields_and_updates_device_view);
    RUN_TEST(test_receive_raw_displays_timings_without_decoding);
    RUN_TEST(test_receive_native_long_protocol_shows_identity_and_state_without_fake_fields);
    RUN_TEST(test_remote_command_delegates_to_shell);
    RUN_TEST(test_record_saves_selected_raw_command_to_littlefs);
    RUN_TEST(test_record_refuses_when_littlefs_has_not_enough_space);
    RUN_TEST(test_load_sends_selected_command_from_littlefs_file);
    RUN_TEST(test_load_reports_missing_ir_files);
    RUN_TEST(test_jam_selects_mode_carrier_and_density_then_stops);
    RUN_TEST(test_unknown_command_displays_help);
    RUN_TEST(test_failed_send_does_not_report_success);
    RUN_TEST(test_load_refuses_malformed_file_command);
    RUN_TEST(test_load_continues_after_mount_and_reports_mount_failure);
    RUN_TEST(test_record_stops_before_size_limit_and_file_can_be_loaded);
    RUN_TEST(test_load_accepts_32k_boundary_and_rejects_empty_or_larger_files);
    RUN_TEST(test_replay_preserves_end_to_start_gaps_and_timer_wraparound);
    RUN_TEST(test_replay_zero_gap_can_be_stopped_and_invalid_counts_do_not_start_rx);
    RUN_TEST(test_replay_caps_total_capture_memory);
    RUN_TEST(test_send_rejects_narrowing_and_extra_arguments);
    RUN_TEST(test_setprotocol_handles_large_numeric_input_and_valid_selection);
    RUN_TEST(test_send_rejects_byte_truncation_but_allows_wide_makehex_protocols);
    RUN_TEST(test_load_streams_one_command_without_readall);
    RUN_TEST(test_load_rejects_oversize_selected_command_without_consuming_all_ram);
}
