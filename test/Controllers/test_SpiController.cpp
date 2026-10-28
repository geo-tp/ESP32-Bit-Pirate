#include <unity.h>

#include <string>
#include <vector>

#include "Controllers/SpiController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeSdService.h"
#include "../Services/FakeSpiService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeShell.h"
#include "../Views/FakeTerminalView.h"

namespace spi_controller_tests {

struct SpiControllerFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeUtilityService utility;
    FakeSpiService spiService;
    FakeSdService sdService;
    ArgTransformer transformer;
    UserInputManager userInput{view, input, transformer};
    BinaryAnalyzer binaryAnalyzer{view, input};
    FakeShell sdCardShell;
    FakeShell flashShell;
    FakeShell eepromShell;
    HelpShell helpShell{view, input, userInput};
    SpiController controller{
        view,
        input,
        utility,
        spiService,
        sdService,
        transformer,
        userInput,
        binaryAnalyzer,
        sdCardShell,
        flashShell,
        eepromShell,
        helpShell
    };

    SpiControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::SPI);
        state.setSpiMOSIPin(14);
        state.setSpiMISOPin(39);
        state.setSpiCLKPin(40);
        state.setSpiCSPin(12);
        state.setSpiWPPin(-1);
        state.setSpiHOLDPin(-1);
        state.setSpiFrequency(20000000);
    }
};

void queueDefaultConfiguration(SpiControllerFixture& fixture) {
    fixture.input.queueLine("");
    fixture.input.queueLine("");
    fixture.input.queueLine("");
    fixture.input.queueLine("");
    fixture.input.queueLine("");
}

void test_config_updates_state_and_configures_spi_service() {
    SpiControllerFixture fixture;
    fixture.input.queueLine("10");
    fixture.input.queueLine("11");
    fixture.input.queueLine("12");
    fixture.input.queueLine("13");
    fixture.input.queueLine("8");

    fixture.controller.handleCommand(TerminalCommand("config"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.configurations.size());
    const auto& config = fixture.spiService.configurations[0];
    TEST_ASSERT_EQUAL_UINT8(10, config.mosi);
    TEST_ASSERT_EQUAL_UINT8(11, config.miso);
    TEST_ASSERT_EQUAL_UINT8(12, config.sclk);
    TEST_ASSERT_EQUAL_UINT8(13, config.cs);
    TEST_ASSERT_EQUAL_UINT32(8000000, config.frequency);
    TEST_ASSERT_EQUAL_INT(-1, config.wp);
    TEST_ASSERT_EQUAL_INT(-1, config.hold);
    TEST_ASSERT_FALSE(fixture.view.contains("WP GPIO"));
    TEST_ASSERT_FALSE(fixture.view.contains("HOLD GPIO"));
    TEST_ASSERT_EQUAL_UINT8(10, GlobalState::getInstance().getSpiMOSIPin());
    TEST_ASSERT_EQUAL_UINT8(11, GlobalState::getInstance().getSpiMISOPin());
    TEST_ASSERT_EQUAL_UINT8(12, GlobalState::getInstance().getSpiCLKPin());
    TEST_ASSERT_EQUAL_UINT8(13, GlobalState::getInstance().getSpiCSPin());
    TEST_ASSERT_EQUAL_UINT32(8000000, GlobalState::getInstance().getSpiFrequency());
    TEST_ASSERT_TRUE(fixture.view.contains("SPI configured."));
}

void test_optional_pins_reject_bus_conflicts_and_are_restored() {
    SpiControllerFixture fixture;
    queueDefaultConfiguration(fixture);
    fixture.input.queueLine("y");
    fixture.input.queueLine("14"); // MOSI is already taken.
    fixture.input.queueLine("10");
    fixture.input.queueLine("10"); // WP is already taken.
    fixture.input.queueLine("11");
    fixture.controller.ensureConfigured();
    fixture.controller.ensureConfigured();
    TEST_ASSERT_TRUE(fixture.input.blockingChars.empty());
    TEST_ASSERT_EQUAL_UINT8(10, fixture.spiService.configurations.back().wp);
    TEST_ASSERT_EQUAL_UINT8(11, fixture.spiService.configurations.back().hold);
    TEST_ASSERT_TRUE(fixture.view.contains("reserved/protected"));
    const std::string prompt = "Use WP or HOLD ?";
    const auto position = fixture.view.output.find(prompt);
    TEST_ASSERT_TRUE(position != std::string::npos);
    TEST_ASSERT_TRUE(fixture.view.output.find(prompt, position + prompt.size()) == std::string::npos);

    fixture.input.queueReadChar('\n');
    fixture.controller.handleCommand(TerminalCommand("slave"));
    TEST_ASSERT_EQUAL_UINT8(10, fixture.spiService.configurations.back().wp);
    TEST_ASSERT_EQUAL_UINT8(11, fixture.spiService.configurations.back().hold);

    queueDefaultConfiguration(fixture);
    fixture.input.queueLine("n");
    fixture.controller.handleCommand(TerminalCommand("config"));
    TEST_ASSERT_TRUE(fixture.input.blockingChars.empty());
    TEST_ASSERT_EQUAL_INT(-1, fixture.spiService.configurations.back().wp);
    TEST_ASSERT_EQUAL_INT(-1, fixture.spiService.configurations.back().hold);
}

void test_optional_pins_can_be_enabled_independently() {
    for (bool useWp : {false, true}) {
        SpiControllerFixture fixture;
        queueDefaultConfiguration(fixture);
        fixture.input.queueLine("y");
        fixture.input.queueLine(useWp ? "10" : "");
        fixture.input.queueLine(useWp ? "-1" : "11");
        fixture.controller.ensureConfigured();
        TEST_ASSERT_TRUE(fixture.input.blockingChars.empty());
        TEST_ASSERT_EQUAL_INT(useWp ? 10 : -1, fixture.spiService.configurations.back().wp);
        TEST_ASSERT_EQUAL_INT(useWp ? -1 : 11, fixture.spiService.configurations.back().hold);
    }
}

void test_empty_optional_pins_disable_saved_pins_and_reject_out_of_range_values() {
    SpiControllerFixture fixture;
    GlobalState::getInstance().setSpiWPPin(10);
    GlobalState::getInstance().setSpiHOLDPin(11);
    queueDefaultConfiguration(fixture);
    fixture.input.queueLine("y");
    fixture.input.queueLine("-2");
    fixture.input.queueLine("49");
    fixture.input.queueLine(""); // Empty always disables, even with a saved pin.
    fixture.input.queueLine("");
    fixture.controller.handleCommand(TerminalCommand("config"));
    TEST_ASSERT_TRUE(fixture.input.blockingChars.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("WP GPIO [-1]"));
    TEST_ASSERT_TRUE(fixture.view.contains("HOLD GPIO [-1]"));
    TEST_ASSERT_EQUAL_INT(-1, fixture.spiService.configurations.back().wp);
    TEST_ASSERT_EQUAL_INT(-1, fixture.spiService.configurations.back().hold);
    TEST_ASSERT_EQUAL_INT(-1, GlobalState::getInstance().getSpiWPPin());
    TEST_ASSERT_EQUAL_INT(-1, GlobalState::getInstance().getSpiHOLDPin());
    TEST_ASSERT_FALSE(fixture.view.contains("held HIGH"));
}

void test_ensure_configured_prompts_once_then_reapplies_saved_state() {
    SpiControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.ensureConfigured();
    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(3, fixture.spiService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(2, fixture.spiService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.sdService.endCalls);

    const auto& reapplied = fixture.spiService.configurations[2];
    TEST_ASSERT_EQUAL_UINT8(14, reapplied.mosi);
    TEST_ASSERT_EQUAL_UINT8(39, reapplied.miso);
    TEST_ASSERT_EQUAL_UINT8(40, reapplied.sclk);
    TEST_ASSERT_EQUAL_UINT8(12, reapplied.cs);
    TEST_ASSERT_EQUAL_UINT32(20000000, reapplied.frequency);
}

void test_instruction_delegates_bytecodes_and_prints_non_empty_result() {
    SpiControllerFixture fixture;
    fixture.spiService.byteCodeResult = "DE AD";
    const std::vector<ByteCode> bytecodes = {
        ByteCode(ByteCodeEnum::Write, 0x9F),
        ByteCode(ByteCodeEnum::Read, 3)
    };

    fixture.controller.handleInstruction(bytecodes);

    TEST_ASSERT_EQUAL_UINT32(2, fixture.spiService.lastBytecodes.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ByteCodeEnum::Write),
                          static_cast<int>(fixture.spiService.lastBytecodes[0].getCommand()));
    TEST_ASSERT_EQUAL_HEX32(0x9F, fixture.spiService.lastBytecodes[0].getData());
    TEST_ASSERT_TRUE(fixture.view.contains("SPI Read:"));
    TEST_ASSERT_TRUE(fixture.view.contains("DE AD"));
}

void test_instruction_keeps_terminal_quiet_for_empty_result() {
    SpiControllerFixture fixture;

    fixture.controller.handleInstruction({ByteCode(ByteCodeEnum::Read, 1)});

    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.lastBytecodes.size());
    TEST_ASSERT_TRUE(fixture.view.output.empty());
}

void test_slave_captures_packets_then_restores_spi_configuration() {
    SpiControllerFixture fixture;
    fixture.spiService.slaveDataBatches.push_back({{0xAA, 0x10}});
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("slave"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.startSlaveCalls.size());
    TEST_ASSERT_EQUAL_INT(40, fixture.spiService.startSlaveCalls[0].sclk);
    TEST_ASSERT_EQUAL_INT(39, fixture.spiService.startSlaveCalls[0].miso);
    TEST_ASSERT_EQUAL_INT(14, fixture.spiService.startSlaveCalls[0].mosi);
    TEST_ASSERT_EQUAL_INT(12, fixture.spiService.startSlaveCalls[0].cs);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.stopSlaveCalls.size());
    TEST_ASSERT_EQUAL_UINT32(2, fixture.spiService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.configurations.size());
    TEST_ASSERT_TRUE(fixture.view.contains("[MOSI] AA 10"));
    TEST_ASSERT_TRUE(fixture.view.contains("SPI Slave: Stopped by user."));
}

void test_sniff_mosi_and_miso_use_expected_slave_pin_mapping() {
    SpiControllerFixture mosi;
    mosi.spiService.slaveDataBatches.push_back({{0x01, 0x02}});
    mosi.input.queueLine("");
    mosi.input.queueReadChar(KEY_NONE);
    mosi.input.queueReadChar('\n');

    mosi.controller.handleCommand(TerminalCommand("sniff"));

    TEST_ASSERT_EQUAL_UINT32(1, mosi.spiService.startSlaveCalls.size());
    TEST_ASSERT_EQUAL_INT(40, mosi.spiService.startSlaveCalls[0].sclk);
    TEST_ASSERT_EQUAL_INT(39, mosi.spiService.startSlaveCalls[0].miso);
    TEST_ASSERT_EQUAL_INT(14, mosi.spiService.startSlaveCalls[0].mosi);
    TEST_ASSERT_EQUAL_INT(12, mosi.spiService.startSlaveCalls[0].cs);
    TEST_ASSERT_TRUE(mosi.view.contains("[MOSI] 01 02"));

    SpiControllerFixture miso;
    miso.spiService.slaveDataBatches.push_back({{0x0A}});
    miso.input.queueLine("2");
    miso.input.queueReadChar(KEY_NONE);
    miso.input.queueReadChar('\n');

    miso.controller.handleCommand(TerminalCommand("sniff"));

    TEST_ASSERT_EQUAL_UINT32(1, miso.spiService.startSlaveCalls.size());
    TEST_ASSERT_EQUAL_INT(40, miso.spiService.startSlaveCalls[0].sclk);
    TEST_ASSERT_EQUAL_INT(-1, miso.spiService.startSlaveCalls[0].miso);
    TEST_ASSERT_EQUAL_INT(39, miso.spiService.startSlaveCalls[0].mosi);
    TEST_ASSERT_EQUAL_INT(12, miso.spiService.startSlaveCalls[0].cs);
    TEST_ASSERT_TRUE(miso.view.contains("[MISO] 0A"));
}

void test_flash_and_eeprom_delegate_to_shells() {
    SpiControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.handleCommand(TerminalCommand("flash"));
    fixture.controller.handleCommand(TerminalCommand("eeprom"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.flashShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.eepromShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.spiService.configurations.size());
}

void test_sdcard_mount_runs_shell_then_restores_spi() {
    SpiControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.handleCommand(TerminalCommand("sdcard"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.sdService.configureCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sdCardShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.sdService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(3, fixture.spiService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.spiService.configurations.size());
    const auto& restored = fixture.spiService.configurations.back();
    TEST_ASSERT_EQUAL_UINT8(14, restored.mosi);
    TEST_ASSERT_EQUAL_UINT8(39, restored.miso);
    TEST_ASSERT_EQUAL_UINT8(40, restored.sclk);
    TEST_ASSERT_EQUAL_UINT8(12, restored.cs);
    TEST_ASSERT_EQUAL_UINT32(20000000, restored.frequency);
    TEST_ASSERT_TRUE(fixture.view.contains("SD Card: Mounted successfully."));
}

void test_sdcard_mount_failure_restores_spi_and_control_pins() {
    SpiControllerFixture fixture;
    fixture.sdService.configureResult = false;
    GlobalState::getInstance().setSpiWPPin(10);
    GlobalState::getInstance().setSpiHOLDPin(11);

    fixture.controller.handleCommand(TerminalCommand("sdcard"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.sdService.configureCalls);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.sdCardShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.spiService.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(10, fixture.spiService.configurations.back().wp);
    TEST_ASSERT_EQUAL_UINT8(11, fixture.spiService.configurations.back().hold);
    TEST_ASSERT_TRUE(fixture.view.contains("SD Card: Mount failed."));
}

void test_unknown_command_displays_spi_help() {
    SpiControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("wat"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available SPI commands"));
    TEST_ASSERT_TRUE(fixture.view.contains("SPI"));
}

}  // namespace spi_controller_tests

void runSpiControllerTests() {
    using namespace spi_controller_tests;
    RUN_TEST(test_config_updates_state_and_configures_spi_service);
    RUN_TEST(test_optional_pins_reject_bus_conflicts_and_are_restored);
    RUN_TEST(test_optional_pins_can_be_enabled_independently);
    RUN_TEST(test_empty_optional_pins_disable_saved_pins_and_reject_out_of_range_values);
    RUN_TEST(test_ensure_configured_prompts_once_then_reapplies_saved_state);
    RUN_TEST(test_instruction_delegates_bytecodes_and_prints_non_empty_result);
    RUN_TEST(test_instruction_keeps_terminal_quiet_for_empty_result);
    RUN_TEST(test_slave_captures_packets_then_restores_spi_configuration);
    RUN_TEST(test_sniff_mosi_and_miso_use_expected_slave_pin_mapping);
    RUN_TEST(test_flash_and_eeprom_delegate_to_shells);
    RUN_TEST(test_sdcard_mount_runs_shell_then_restores_spi);
    RUN_TEST(test_sdcard_mount_failure_restores_spi_and_control_pins);
    RUN_TEST(test_unknown_command_displays_spi_help);
}
