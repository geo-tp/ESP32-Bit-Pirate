#include <unity.h>

#include "Controllers/UtilityController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeI2sService.h"
#include "../Services/FakePinService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeShell.h"
#include "../Views/FakeDeviceView.h"
#include "../Views/FakeTerminalView.h"

namespace utility_controller_tests {

struct UtilityControllerFixture {
    FakeTerminalView view;
    FakeDeviceView device;
    FakeInput input;
    FakeUtilityService utility;
    FakePinService pinService;
    FakeI2sService i2sService;
    ArgTransformer argTransformer;
    TerminalCommandTransformer commandTransformer;
    UserInputManager userInput{view, input, argTransformer};
    PinAnalyzer pinAnalyzer{pinService, utility};
    AliasManager aliasManager;
    FakeShell sysInfoShell;
    FakeShell guideShell;
    FakeShell profileShell;
    HelpShell helpShell{view, input, userInput};
    UtilityController controller{
        view,
        device,
        input,
        utility,
        pinService,
        i2sService,
        userInput,
        pinAnalyzer,
        aliasManager,
        argTransformer,
        commandTransformer,
        sysInfoShell,
        guideShell,
        helpShell,
        profileShell
    };

    UtilityControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::HIZ);
        state.setTerminalMode(TerminalTypeEnum::Standalone);
        state.setI2cSdaPin(4);
        state.setI2cSclPin(5);
        state.setSpiMISOPin(6);
        state.setOneWirePin(7);
        state.setUartRxPin(8);
        state.setHdUartPin(9);
        state.setTwoWireIoPin(10);
        state.setJtagScanPins({11, 12, 13, 14});
        state.setI2sBclkPin(41);
        state.setI2sLrckPin(43);
        state.setI2sDataPin(42);
        state.setI2sSampleRate(44100);
        state.setI2sBitsPerSample(16);
        state.setI2sPercentLevel(50);
    }
};

void test_system_guide_and_profile_delegate_to_interfaces() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("system"));
    fixture.controller.handleCommand(TerminalCommand("guide"));
    fixture.controller.handleCommand(TerminalCommand("profile"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.sysInfoShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.guideShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.profileShell.runCalls);
}

void test_mode_command_accepts_known_mode_and_rejects_unknown_mode() {
    UtilityControllerFixture fixture;

    const ModeEnum i2c = fixture.controller.handleModeChangeCommand(TerminalCommand("mode", "i2c"));
    const ModeEnum unknown = fixture.controller.handleModeChangeCommand(TerminalCommand("mode", "wat"));
    const ModeEnum invalidRoot = fixture.controller.handleModeChangeCommand(TerminalCommand("wat", "i2c"));

    TEST_ASSERT_EQUAL(ModeEnum::I2C, i2c);
    TEST_ASSERT_EQUAL(ModeEnum::None, unknown);
    TEST_ASSERT_EQUAL(ModeEnum::None, invalidRoot);
    TEST_ASSERT_TRUE(fixture.view.contains("Mode changed to I2C"));
    TEST_ASSERT_TRUE(fixture.view.contains("Unknown mode"));
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid command"));
}

void test_mode_select_uses_user_choice() {
    UtilityControllerFixture fixture;
    fixture.input.queueLine("1");

    const ModeEnum selected = fixture.controller.handleModeChangeCommand(TerminalCommand("mode"));

    TEST_ASSERT_EQUAL(ModeEnum::HIZ, selected);
    TEST_ASSERT_TRUE(fixture.view.contains("Mode changed to HIZ"));
}

void test_mode_select_reports_empty_input_as_invalid_input() {
    UtilityControllerFixture fixture;
    fixture.input.queueLine("");

    const ModeEnum selected = fixture.controller.handleModeChangeCommand(TerminalCommand("mode"));

    TEST_ASSERT_EQUAL(ModeEnum::None, selected);
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid input."));
}

void test_pullup_commands_apply_to_current_mode_pins() {
    UtilityControllerFixture fixture;
    GlobalState::getInstance().setCurrentMode(ModeEnum::I2C);

    fixture.controller.handleCommand(TerminalCommand("P"));
    fixture.controller.handleCommand(TerminalCommand("p"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.pinService.inputPullupCalls.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.pinService.inputPullupCalls[0]);
    TEST_ASSERT_EQUAL_UINT8(5, fixture.pinService.inputPullupCalls[1]);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.pinService.inputCalls.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.pinService.inputCalls[0]);
    TEST_ASSERT_EQUAL_UINT8(5, fixture.pinService.inputCalls[1]);
    TEST_ASSERT_TRUE(fixture.view.contains("Pull-ups enabled on SDA, SCL"));
    TEST_ASSERT_TRUE(fixture.view.contains("Pull-ups disabled on SDA, SCL"));
}

void test_hex_command_formats_decimal_hex_binary_and_ascii() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("hex", "65"));

    TEST_ASSERT_TRUE(fixture.view.contains("Dec   : 65"));
    TEST_ASSERT_TRUE(fixture.view.contains("Hex   : 0x41"));
    TEST_ASSERT_TRUE(fixture.view.contains("ASCII : A"));
}

void test_hex_rejects_invalid_number() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("hex", "12abc"));

    TEST_ASSERT_TRUE(fixture.view.contains("Usage: hex <number>"));
}

void test_delay_variants_sleep_expected_microseconds() {
    UtilityControllerFixture seconds;
    seconds.controller.handleCommand(TerminalCommand("delay", "0"));

    UtilityControllerFixture milliseconds;
    milliseconds.controller.handleCommand(TerminalCommand("delayms", "1"));

    UtilityControllerFixture microseconds;
    microseconds.controller.handleCommand(TerminalCommand("delayus", "10"));

    TEST_ASSERT_EQUAL_UINT32(0, seconds.utility.sleepUsCalls);
    TEST_ASSERT_EQUAL_UINT32(1, milliseconds.utility.sleepUsCalls);
    TEST_ASSERT_EQUAL_UINT32(1000, milliseconds.utility.lastSleepUs);
    TEST_ASSERT_EQUAL_UINT32(1, microseconds.utility.sleepUsCalls);
    TEST_ASSERT_EQUAL_UINT32(10, microseconds.utility.lastSleepUs);
}

void test_delay_reports_usage_for_invalid_value() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("delay", "wat"));

    TEST_ASSERT_TRUE(fixture.view.contains("Usage: delay <seconds>"));
}

void test_long_delay_can_be_cancelled_before_sleeping() {
    UtilityControllerFixture fixture;
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("delayms", "25"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.utility.sleepUsCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Delaying for 25 ms"));
}

void test_alias_creates_shortcut_and_rejects_builtin_names() {
    UtilityControllerFixture created;
    created.input.queueLine("i2c scan");
    created.input.queueLine("scanbus");
    created.controller.handleCommand(TerminalCommand("alias"));

    UtilityControllerFixture builtin;
    builtin.input.queueLine("i2c scan");
    builtin.input.queueLine("help");
    builtin.controller.handleCommand(TerminalCommand("alias"));

    TEST_ASSERT_EQUAL_STRING("i2c scan", created.aliasManager.expand("scanbus").c_str());
    TEST_ASSERT_TRUE(created.view.contains("Alias: Set"));
    TEST_ASSERT_FALSE(builtin.aliasManager.has("help"));
    TEST_ASSERT_TRUE(builtin.view.contains("built-in command"));
}

void test_logic_and_analogic_validate_arguments_before_sampling() {
    UtilityControllerFixture logic;
    logic.controller.handleCommand(TerminalCommand("logic"));

    UtilityControllerFixture analogic;
    analogic.controller.handleCommand(TerminalCommand("analogic", "20"));

    TEST_ASSERT_TRUE(logic.view.contains("Usage: logic <gpio>"));
    TEST_ASSERT_TRUE(analogic.view.contains("not an analog one"));
}

void test_logic_analyzer_draws_trace_then_stops() {
    UtilityControllerFixture fixture;
    fixture.pinService.defaultRead = true;
    fixture.input.queueReadChar('\n');
    fixture.utility.queueNowMs(0);
    for (int i = 0; i < 321; ++i) {
        fixture.utility.queueNowMs(0);
    }
    fixture.utility.queueNowMs(11);
    fixture.utility.queueNowMs(11);

    fixture.controller.handleCommand(TerminalCommand("logic", "16"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.pinService.inputCalls.size());
    TEST_ASSERT_EQUAL_UINT8(16, fixture.pinService.inputCalls[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.device.logicTraceCalls.size());
    TEST_ASSERT_EQUAL_UINT32(320, fixture.device.logicTraceCalls[0].samples.size());
    TEST_ASSERT_EQUAL_UINT8(16, fixture.device.logicTraceCalls[0].pin);
    TEST_ASSERT_TRUE(fixture.view.contains("Logic Analyzer: Stopped by user."));
}

void test_analogic_draws_trace_then_stops() {
    UtilityControllerFixture fixture;
    fixture.pinService.analogValue = 2048;
    fixture.input.queueReadChar('\n');
    fixture.utility.queueNowMs(0);
    for (int i = 0; i < 321; ++i) {
        fixture.utility.queueNowMs(0);
    }
    fixture.utility.queueNowMs(11);
    fixture.utility.queueNowMs(11);

    fixture.controller.handleCommand(TerminalCommand("analogic", "15"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.pinService.inputCalls.size());
    TEST_ASSERT_EQUAL_UINT8(15, fixture.pinService.inputCalls[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.device.analogicTraceCalls.size());
    TEST_ASSERT_EQUAL_UINT32(320, fixture.device.analogicTraceCalls[0].samples.size());
    TEST_ASSERT_EQUAL_UINT8(15, fixture.device.analogicTraceCalls[0].pin);
    TEST_ASSERT_EQUAL_UINT8(128, fixture.device.analogicTraceCalls[0].samples[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("Analogic: Stopped by user."));
}

void test_wizard_samples_once_and_stops_on_enter() {
    UtilityControllerFixture fixture;
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("wizard", "16"));

    TEST_ASSERT_FALSE(fixture.pinService.readCalls.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Wizard: Please wait, analyzing GPIO 16"));
    TEST_ASSERT_TRUE(fixture.view.contains("Wizard: Stopped by user."));
}

void test_listen_configures_audio_and_stops_on_enter() {
    UtilityControllerFixture fixture;
    fixture.pinService.pulls[3] = IPinService::PULL_UP;

    fixture.controller.handleCommand(TerminalCommand("listen", "3"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.pinService.inputPullupCalls.size());
    TEST_ASSERT_EQUAL_UINT8(3, fixture.pinService.inputPullupCalls[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2sService.outputConfigureCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Listen: Activity to Audio @ GPIO 3"));
    TEST_ASSERT_TRUE(fixture.view.contains("Listen: Stopped by user"));
}

void test_listen_rejects_i2s_owned_pin() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("listen", "41"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.i2sService.outputConfigureCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("used by I2S"));
}

void test_unknown_command_displays_current_mode_help() {
    UtilityControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("wat"));

    TEST_ASSERT_TRUE(fixture.view.contains("General"));
    TEST_ASSERT_TRUE(fixture.view.contains("help                 - Show this help"));
}

}  // namespace utility_controller_tests

void runUtilityControllerTests() {
    using namespace utility_controller_tests;
    RUN_TEST(test_system_guide_and_profile_delegate_to_interfaces);
    RUN_TEST(test_mode_command_accepts_known_mode_and_rejects_unknown_mode);
    RUN_TEST(test_mode_select_uses_user_choice);
    RUN_TEST(test_mode_select_reports_empty_input_as_invalid_input);
    RUN_TEST(test_pullup_commands_apply_to_current_mode_pins);
    RUN_TEST(test_hex_command_formats_decimal_hex_binary_and_ascii);
    RUN_TEST(test_hex_rejects_invalid_number);
    RUN_TEST(test_delay_variants_sleep_expected_microseconds);
    RUN_TEST(test_delay_reports_usage_for_invalid_value);
    RUN_TEST(test_long_delay_can_be_cancelled_before_sleeping);
    RUN_TEST(test_alias_creates_shortcut_and_rejects_builtin_names);
    RUN_TEST(test_logic_and_analogic_validate_arguments_before_sampling);
    RUN_TEST(test_logic_analyzer_draws_trace_then_stops);
    RUN_TEST(test_analogic_draws_trace_then_stops);
    RUN_TEST(test_wizard_samples_once_and_stops_on_enter);
    RUN_TEST(test_listen_configures_audio_and_stops_on_enter);
    RUN_TEST(test_listen_rejects_i2s_owned_pin);
    RUN_TEST(test_unknown_command_displays_current_mode_help);
}
