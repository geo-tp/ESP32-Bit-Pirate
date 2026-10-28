#include <unity.h>

#include <algorithm>

#include "Controllers/JtagController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeJtagService.h"
#include "../Shells/FakeUsbAdapterShell.h"
#include "../Views/FakeTerminalView.h"

namespace jtag_controller_tests {

struct JtagControllerFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeJtagService jtagService;
    FakeUsbAdapterShell usbAdapterShell;
    ArgTransformer transformer;
    UserInputManager userInput{view, input, transformer};
    HelpShell helpShell{view, input, userInput};
    JtagController controller{
        view,
        input,
        jtagService,
        userInput,
        helpShell,
        usbAdapterShell
    };

    JtagControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::JTAG);
        state.setJtagScanPins({1, 3, 5, 7, 9});
    }
};

void test_jtag_scan_requires_swd_or_jtag_subcommand() {
    JtagControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.jtagService.swdScanCalls);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.jtagService.jtagScanCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("scan swd"));
}

void test_jtag_swd_scan_displays_pins_and_hex_idcode() {
    JtagControllerFixture fixture;
    fixture.jtagService.swdFound = true;
    fixture.jtagService.swdio = 4;
    fixture.jtagService.swclk = 6;
    fixture.jtagService.swdIdcode = 0x2BA01477;

    fixture.controller.handleCommand(TerminalCommand("scan", "swd"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.jtagService.swdScanCalls);
    TEST_ASSERT_EQUAL_UINT32(5, fixture.jtagService.lastCandidates.size());
    TEST_ASSERT_TRUE(fixture.view.contains("SWDIO  : GPIO 4"));
    TEST_ASSERT_TRUE(fixture.view.contains("SWCLK  : GPIO 6"));
    TEST_ASSERT_TRUE(fixture.view.contains("IDCODE : 0x2BA01477"));
}

void test_jtag_swd_scan_reports_no_device() {
    JtagControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan", "swd"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.jtagService.swdScanCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("No SWD device found"));
}

void test_jtag_scan_displays_pin_mapping_trst_and_device_ids() {
    JtagControllerFixture fixture;
    fixture.jtagService.jtagFound = true;
    fixture.jtagService.tdi = 1;
    fixture.jtagService.tdo = 3;
    fixture.jtagService.tck = 5;
    fixture.jtagService.tms = 7;
    fixture.jtagService.trst = 9;
    fixture.jtagService.jtagIds = {0x12345678, 0xABCDEF01};

    fixture.controller.handleCommand(TerminalCommand("scan", "jtag"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.jtagService.jtagScanCalls);
    TEST_ASSERT_FALSE(fixture.jtagService.lastPulsePins);
    TEST_ASSERT_TRUE(fixture.view.contains("TDI   : GPIO 1"));
    TEST_ASSERT_TRUE(fixture.view.contains("TRST  : GPIO 9"));
    TEST_ASSERT_TRUE(fixture.view.contains("IDCODE[0] : 0x12345678"));
    TEST_ASSERT_TRUE(fixture.view.contains("IDCODE[1] : 0xABCDEF01"));
}

void test_jtag_scan_omits_trst_when_service_does_not_find_it() {
    JtagControllerFixture fixture;
    fixture.jtagService.jtagFound = true;
    fixture.jtagService.trst = -1;

    fixture.controller.handleCommand(TerminalCommand("scan", "jtag"));

    TEST_ASSERT_FALSE(fixture.view.contains("TRST  : GPIO"));
}

void test_jtag_scan_reports_no_jtag_device() {
    JtagControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan", "jtag"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.jtagService.jtagScanCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("No device found"));
}

void test_jtag_scan_retries_with_pulsing_when_normal_scan_fails() {
    JtagControllerFixture fixture;
    fixture.jtagService.jtagFoundWithPulsing = true;
    fixture.jtagService.jtagIds = {0x13722093};

    fixture.controller.handleCommand(TerminalCommand("scan", "jtag"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.jtagService.jtagScanCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.jtagService.pulseModes.size());
    TEST_ASSERT_FALSE(fixture.jtagService.pulseModes[0]);
    TEST_ASSERT_TRUE(fixture.jtagService.pulseModes[1]);
    TEST_ASSERT_TRUE(fixture.view.contains("JTAG device(s) found"));
    TEST_ASSERT_TRUE(fixture.view.contains("0x13722093"));
    TEST_ASSERT_FALSE(fixture.view.contains("No device found"));
}

void test_jtag_config_saves_selected_scan_pins() {
    JtagControllerFixture fixture;
    fixture.input.queueLine("2 4 6 8");

    fixture.controller.handleCommand(TerminalCommand("config"));

    const auto& pins = GlobalState::getInstance().getJtagScanPins();
    const uint8_t expected[] = {2, 4, 6, 8};
    TEST_ASSERT_EQUAL_UINT32(4, pins.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, pins.data(), 4);
    TEST_ASSERT_TRUE(fixture.view.contains("GPIOs set for the scan"));
}

void test_jtag_ensure_configured_prompts_only_once() {
    JtagControllerFixture fixture;
    fixture.input.queueLine("");

    fixture.controller.ensureConfigured();
    fixture.controller.ensureConfigured();

    const auto prompts = std::count_if(
        fixture.view.printCalls.begin(), fixture.view.printCalls.end(),
        [](const std::string& value) { return value.find("GPIOs to scan") != std::string::npos; });
    TEST_ASSERT_EQUAL_INT(1, prompts);
}

void test_jtag_openocd_can_be_cancelled() {
    JtagControllerFixture fixture;
    fixture.input.queueLine("n");

    fixture.controller.handleCommand(TerminalCommand("openocd"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.usbAdapterShell.rebootOpenOcdCalls);
}

void test_jtag_openocd_confirmation_delegates_reboot() {
    JtagControllerFixture fixture;
    fixture.input.queueLine("y");

    fixture.controller.handleCommand(TerminalCommand("openocd"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.usbAdapterShell.rebootOpenOcdCalls);
}

void test_jtag_unknown_command_displays_help() {
    JtagControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("unknown"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available JTAG commands"));
    TEST_ASSERT_TRUE(fixture.view.contains("scan swd"));
    TEST_ASSERT_TRUE(fixture.view.contains("scan jtag"));
}

}  // namespace jtag_controller_tests

void runJtagControllerTests() {
    using namespace jtag_controller_tests;
    RUN_TEST(test_jtag_scan_requires_swd_or_jtag_subcommand);
    RUN_TEST(test_jtag_swd_scan_displays_pins_and_hex_idcode);
    RUN_TEST(test_jtag_swd_scan_reports_no_device);
    RUN_TEST(test_jtag_scan_displays_pin_mapping_trst_and_device_ids);
    RUN_TEST(test_jtag_scan_omits_trst_when_service_does_not_find_it);
    RUN_TEST(test_jtag_scan_reports_no_jtag_device);
    RUN_TEST(test_jtag_scan_retries_with_pulsing_when_normal_scan_fails);
    RUN_TEST(test_jtag_config_saves_selected_scan_pins);
    RUN_TEST(test_jtag_ensure_configured_prompts_only_once);
    RUN_TEST(test_jtag_openocd_can_be_cancelled);
    RUN_TEST(test_jtag_openocd_confirmation_delegates_reboot);
    RUN_TEST(test_jtag_unknown_command_displays_help);
}
