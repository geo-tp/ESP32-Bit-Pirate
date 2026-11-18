#include <array>
#include <unity.h>

#include "Controllers/OneWireController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeOneWireService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeShell.h"
#include "../Views/FakeTerminalView.h"

namespace one_wire_controller_tests {

struct OneWireControllerFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeUtilityService utility;
    FakeOneWireService service;
    ArgTransformer transformer;
    UserInputManager userInput{view, input, transformer};
    FakeShell ibuttonShell;
    FakeShell eepromShell;
    HelpShell helpShell{view, input, userInput};
    OneWireController controller{
        view,
        input,
        utility,
        service,
        transformer,
        userInput,
        ibuttonShell,
        eepromShell,
        helpShell
    };

    OneWireControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::OneWire);
        state.setOneWirePin(4);
        state.setTerminalMode(TerminalTypeEnum::Standalone);
    }
};

void test_ping_reports_presence_and_absence() {
    OneWireControllerFixture present;
    present.service.defaultResetResult = true;
    present.controller.handleCommand(TerminalCommand("ping"));

    OneWireControllerFixture absent;
    absent.service.defaultResetResult = false;
    absent.controller.handleCommand(TerminalCommand("ping"));

    TEST_ASSERT_TRUE(present.view.contains("Device present"));
    TEST_ASSERT_TRUE(absent.view.contains("No device found"));
}

void test_scan_lists_detected_roms() {
    OneWireControllerFixture fixture;
    fixture.service.searchRoms.push_back({0x28, 0xFF, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11});
    fixture.service.searchRoms.push_back({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.resetSearchCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Device 1: 28 FF AA BB CC DD EE 11"));
    TEST_ASSERT_TRUE(fixture.view.contains("Device 2: 01 02 03 04 05 06 07 08"));
}

void test_scan_reports_empty_bus() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_TRUE(fixture.view.contains("No devices found"));
}

void test_config_updates_state_and_configures_service() {
    OneWireControllerFixture fixture;
    fixture.input.queueLine("7");

    fixture.controller.handleCommand(TerminalCommand("config"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.configuredPins.size());
    TEST_ASSERT_EQUAL_UINT8(7, fixture.service.configuredPins[0]);
    TEST_ASSERT_EQUAL_UINT8(7, GlobalState::getInstance().getOneWirePin());
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire configured"));
}

void test_ensure_configured_prompts_once_then_reapplies_saved_pin() {
    OneWireControllerFixture fixture;
    fixture.input.queueLine("6");

    fixture.controller.ensureConfigured();
    GlobalState::getInstance().setOneWirePin(9);
    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(2, fixture.service.configuredPins.size());
    TEST_ASSERT_EQUAL_UINT8(6, fixture.service.configuredPins[0]);
    TEST_ASSERT_EQUAL_UINT8(9, fixture.service.configuredPins[1]);
}

void test_instruction_delegates_bytecodes_and_displays_result() {
    OneWireControllerFixture fixture;
    fixture.service.byteCodeResult = "AA BB";

    fixture.controller.handleInstruction({ByteCode(ByteCodeEnum::Write, 0xCC)});

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.lastBytecodes.size());
    TEST_ASSERT_EQUAL_HEX32(0xCC, fixture.service.lastBytecodes[0].getData());
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Read"));
    TEST_ASSERT_TRUE(fixture.view.contains("AA BB"));
}

void test_read_can_be_stopped_before_bus_access() {
    OneWireControllerFixture fixture;
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("read"));

    TEST_ASSERT_TRUE(fixture.service.writes.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Read: Stopped by user."));
}

void test_read_executes_rom_and_scratchpad_paths_without_crc_noise() {
    OneWireControllerFixture fixture;
    fixture.input.queueReadChar(KEY_NONE);
    fixture.service.resetResults.push_back(true);
    fixture.service.resetResults.push_back(true);
    fixture.service.readBytesResults.push_back({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    fixture.service.readBytesResults.push_back({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80});

    fixture.controller.handleCommand(TerminalCommand("read"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.service.writes.size());
    TEST_ASSERT_EQUAL_HEX8(0x33, fixture.service.writes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, fixture.service.writes[1]);
    TEST_ASSERT_TRUE(fixture.view.contains("ROM ID: 01 02 03 04 05 06 07 08"));
    TEST_ASSERT_TRUE(fixture.view.contains("Scratchpad: 10 20 30 40 50 60 70 80"));
    TEST_ASSERT_FALSE(fixture.view.contains("CRC error on scratchpad."));
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Read: Complete."));
}

void test_temperature_reports_detected_ds18b20_value() {
    OneWireControllerFixture fixture;
    fixture.service.searchRoms.push_back({0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x10});
    fixture.service.resetResults.push_back(true);
    fixture.service.resetResults.push_back(true);
    fixture.service.readBytesResults.push_back({0x90, 0x01, 0, 0, 0, 0, 0, 0, 0});

    fixture.controller.handleCommand(TerminalCommand("temp"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.resetSearchCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.service.selectedRoms.size());
    TEST_ASSERT_EQUAL_HEX8(0x44, fixture.service.writes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, fixture.service.writes[1]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.utility.sleepMsCalls);
    TEST_ASSERT_EQUAL_UINT32(750, fixture.utility.lastSleepMs);
    TEST_ASSERT_TRUE(fixture.view.contains("Temperature: 25.00"));
}

void test_temperature_reports_reset_failure_before_conversion() {
    OneWireControllerFixture fixture;
    fixture.service.searchRoms.push_back({0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x10});
    fixture.service.resetResults.push_back(false);

    fixture.controller.handleCommand(TerminalCommand("temp"));

    TEST_ASSERT_TRUE(fixture.service.writes.empty());
    TEST_ASSERT_TRUE(fixture.service.selectedRoms.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Temp: Reset failed."));
}

void test_temperature_reports_no_supported_sensor() {
    OneWireControllerFixture fixture;
    fixture.service.searchRoms.push_back({0x01, 0, 0, 0, 0, 0, 0, 0});

    fixture.controller.handleCommand(TerminalCommand("temp"));

    TEST_ASSERT_TRUE(fixture.view.contains("No DS18B20 device found"));
}

void test_write_id_rejects_wrong_length() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("write", "id", "AA BB"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.service.writeRw1990Calls);
    TEST_ASSERT_TRUE(fixture.view.contains("Must be exactly 8 bytes"));
}

void test_write_rejects_unknown_subcommand() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("write", "foo"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.service.writeRw1990Calls);
    TEST_ASSERT_TRUE(fixture.service.writeBytesCalls.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Write: Invalid syntax."));
}

void test_write_id_can_be_aborted_waiting_for_device() {
    OneWireControllerFixture fixture;
    fixture.service.resetResults.push_back(false);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("write", "id", "01 02 03 04 05 06 07 08"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.service.writeRw1990Calls);
    TEST_ASSERT_TRUE(fixture.view.contains("OneWire Write: Stopped by user."));
}

void test_write_id_waits_for_device_writes_and_verifies() {
    OneWireControllerFixture fixture;
    fixture.service.resetResults.push_back(true);
    fixture.service.resetResults.push_back(true);
    fixture.service.readBytesResults.push_back({1, 2, 3, 4, 5, 6, 7, 8});

    fixture.controller.handleCommand(TerminalCommand("write", "id", "01 02 03 04 05 06 07 08"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.writeRw1990Calls);
    TEST_ASSERT_EQUAL_UINT8(4, fixture.service.lastRw1990Pin);
    TEST_ASSERT_EQUAL_UINT32(8, fixture.service.lastRw1990Data.size());
    TEST_ASSERT_EQUAL_HEX8(0x33, fixture.service.writes[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("ID write successful"));
}

void test_ibutton_command_delegates_to_shell() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("ibutton"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.ibuttonShell.runCalls);
}

void test_eeprom_command_wraps_shell_with_eeprom_service_lifecycle() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("eeprom"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.closeCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.configuredEepromPins.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.service.configuredEepromPins[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.eepromShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.closeEepromCalls);
    TEST_ASSERT_FALSE(fixture.service.configuredPins.empty());
}

void test_unknown_command_displays_help() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("wat"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available 1Wire commands"));
}

void test_xfer_reports_usage_without_read_count() {
    OneWireControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("xfer"));

    TEST_ASSERT_TRUE(fixture.view.contains("Usage: xfer <read_count> [0xB1 ... 0xBn]"));
}

void test_xfer_reports_no_device_when_reset_fails() {
    OneWireControllerFixture fixture;
    fixture.service.defaultResetResult = false;

    fixture.controller.handleCommand(TerminalCommand("xfer", "8 0x33"));

    TEST_ASSERT_TRUE(fixture.view.contains("1WIRE XFER: no device"));
    TEST_ASSERT_TRUE(fixture.service.writes.empty());
}

void test_xfer_writes_bytes_then_reads_count_hex() {
    OneWireControllerFixture fixture;
    fixture.service.defaultResetResult = true;
    for (uint8_t b : {0x28, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x56}) {
        fixture.service.readResults.push_back(b);
    }

    fixture.controller.handleCommand(TerminalCommand("xfer", "8 0x33 0xAA"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.service.writes.size());
    TEST_ASSERT_EQUAL_UINT8(0x33, fixture.service.writes[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, fixture.service.writes[1]);
    TEST_ASSERT_TRUE(fixture.view.contains("1WIRE XFER: 28 11 22 33 44 55 66 56"));
}

void test_xfer_zero_read_count_skips_reads() {
    OneWireControllerFixture fixture;
    fixture.service.defaultResetResult = true;

    fixture.controller.handleCommand(TerminalCommand("xfer", "0 0x55"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.service.writes.size());
    TEST_ASSERT_EQUAL_UINT8(0x55, fixture.service.writes[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("1WIRE XFER: 0 bytes read"));
}

}  // namespace one_wire_controller_tests

void runOneWireControllerTests() {
    using namespace one_wire_controller_tests;
    RUN_TEST(test_ping_reports_presence_and_absence);
    RUN_TEST(test_scan_lists_detected_roms);
    RUN_TEST(test_scan_reports_empty_bus);
    RUN_TEST(test_config_updates_state_and_configures_service);
    RUN_TEST(test_ensure_configured_prompts_once_then_reapplies_saved_pin);
    RUN_TEST(test_instruction_delegates_bytecodes_and_displays_result);
    RUN_TEST(test_read_can_be_stopped_before_bus_access);
    RUN_TEST(test_read_executes_rom_and_scratchpad_paths_without_crc_noise);
    RUN_TEST(test_temperature_reports_detected_ds18b20_value);
    RUN_TEST(test_temperature_reports_reset_failure_before_conversion);
    RUN_TEST(test_temperature_reports_no_supported_sensor);
    RUN_TEST(test_write_id_rejects_wrong_length);
    RUN_TEST(test_write_rejects_unknown_subcommand);
    RUN_TEST(test_write_id_can_be_aborted_waiting_for_device);
    RUN_TEST(test_write_id_waits_for_device_writes_and_verifies);
    RUN_TEST(test_ibutton_command_delegates_to_shell);
    RUN_TEST(test_eeprom_command_wraps_shell_with_eeprom_service_lifecycle);
    RUN_TEST(test_xfer_reports_usage_without_read_count);
    RUN_TEST(test_xfer_reports_no_device_when_reset_fails);
    RUN_TEST(test_xfer_writes_bytes_then_reads_count_hex);
    RUN_TEST(test_xfer_zero_read_count_skips_reads);
    RUN_TEST(test_unknown_command_displays_help);
}
