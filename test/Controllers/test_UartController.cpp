#include <unity.h>

#include <string>
#include <vector>

#include "Controllers/UartController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeHdUartService.h"
#include "../Services/FakeSdService.h"
#include "../Services/FakeUartService.h"
#include "../Services/FakeUartSnifferService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeShell.h"
#include "../Views/FakeDeviceView.h"
#include "../Views/FakeTerminalView.h"

namespace uart_controller_tests {

struct UartControllerFixture {
    FakeTerminalView view;
    FakeDeviceView device;
    FakeInput terminalInput;
    FakeInput deviceInput;
    FakeUtilityService utility;
    FakeUartService uartService;
    FakeSdService sdService;
    FakeHdUartService hdUartService;
    FakeUartSnifferService uartSnifferService;
    ArgTransformer transformer;
    UserInputManager userInput{view, terminalInput, transformer};
    FakeShell atShell;
    HelpShell helpShell{view, terminalInput, userInput};
    FakeShell emulationShell;
    UartController controller{
        view,
        terminalInput,
        device,
        deviceInput,
        utility,
        uartService,
        sdService,
        hdUartService,
        uartSnifferService,
        transformer,
        userInput,
        atShell,
        helpShell,
        emulationShell
    };

    UartControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::UART);
        state.setUartRxPin(4);
        state.setUartTxPin(5);
        state.setUartBaudRate(9600);
        state.setUartConfig(0x800001c);
        state.setUartDataBits(8);
        state.setUartParity("N");
        state.setUartStopBits(1);
        state.setUartInverted(false);
    }
};

void queueDefaultConfiguration(UartControllerFixture& fixture) {
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueLine("");
}

void queueCustomConfiguration(UartControllerFixture& fixture) {
    fixture.terminalInput.queueLine("6");
    fixture.terminalInput.queueLine("7");
    fixture.terminalInput.queueLine("19200");
    fixture.terminalInput.queueLine("7");
    fixture.terminalInput.queueLine("e");
    fixture.terminalInput.queueLine("2");
    fixture.terminalInput.queueLine("y");
}

void test_config_applies_selected_serial_parameters() {
    UartControllerFixture fixture;
    fixture.uartService.uartConfigResult = 0x12345678;
    queueCustomConfiguration(fixture);

    fixture.controller.handleCommand(TerminalCommand("config"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.configurations.size());
    const auto& config = fixture.uartService.configurations[0];
    TEST_ASSERT_EQUAL_UINT32(19200, config.baud);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, config.config);
    TEST_ASSERT_EQUAL_UINT8(6, config.rx);
    TEST_ASSERT_EQUAL_UINT8(7, config.tx);
    TEST_ASSERT_TRUE(config.inverted);
    TEST_ASSERT_EQUAL_UINT8(7, fixture.uartService.lastDataBits);
    TEST_ASSERT_EQUAL_CHAR('E', fixture.uartService.lastParity);
    TEST_ASSERT_EQUAL_UINT8(2, fixture.uartService.lastStopBits);
    TEST_ASSERT_EQUAL_UINT8(6, GlobalState::getInstance().getUartRxPin());
    TEST_ASSERT_EQUAL_UINT8(7, GlobalState::getInstance().getUartTxPin());
    TEST_ASSERT_TRUE(fixture.view.contains("UART configured."));
}

void test_ensure_configured_configures_once_and_skips_redundant_reinit() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.ensureConfigured();
    fixture.controller.ensureConfigured();

    // A redundant end()+configure() floats the TX pin (gpio_reset_pin) and
    // the target board receives spurious bytes, so the second call must
    // be a no-op when the stored config is unchanged.
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(0, fixture.uartService.endCalls);
}

void test_ensure_configured_reapplies_when_saved_state_changes() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.ensureConfigured();
    GlobalState::getInstance().setUartBaudRate(19200);
    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(2, fixture.uartService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(19200, fixture.uartService.configurations[1].baud);
}

void test_on_mode_enter_reinitializes_uart_unconditionally() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.ensureConfigured();
    fixture.controller.onModeEnter();

    // Mode entry must always re-establish pin ownership (other modes may
    // have reassigned the pins in the GPIO matrix), even when the config
    // is unchanged.
    TEST_ASSERT_EQUAL_UINT32(2, fixture.uartService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.endCalls);
}

void test_uread_reports_hex_bytes_within_timeout() {
    UartControllerFixture fixture;
    fixture.uartService.rxData.push_back('P');
    fixture.uartService.rxData.push_back('O');
    fixture.uartService.rxData.push_back('N');
    // start=0; three read iterations at t=0; timeout reached at t=10.
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(10);

    fixture.controller.handleCommand(TerminalCommand("uread", "8 10"));

    TEST_ASSERT_TRUE(fixture.view.contains("UART READ: 504F4E"));
}

void test_uread_reports_none_when_bus_idle() {
    UartControllerFixture fixture;
    // start=0; two idle iterations at t=0; timeout reached at t=10.
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(10);

    fixture.controller.handleCommand(TerminalCommand("uread", "8 10"));

    TEST_ASSERT_TRUE(fixture.view.contains("UART READ: none"));
}

void test_instruction_delegates_bytecodes_and_clears_buffer_when_data_is_read() {
    UartControllerFixture fixture;
    fixture.uartService.byteCodeResult = "AA BB";
    const std::vector<ByteCode> bytecodes = {
        ByteCode(ByteCodeEnum::Write, 0x42),
        ByteCode(ByteCodeEnum::Read, 2)
    };

    fixture.controller.handleInstruction(bytecodes);

    TEST_ASSERT_EQUAL_UINT32(2, fixture.uartService.lastBytecodes.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ByteCodeEnum::Write),
                          static_cast<int>(fixture.uartService.lastBytecodes[0].getCommand()));
    TEST_ASSERT_EQUAL_HEX32(0x42, fixture.uartService.lastBytecodes[0].getData());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.clearCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("UART Read:"));
    TEST_ASSERT_TRUE(fixture.view.contains("AA BB"));
}

void test_instruction_reports_no_data_for_empty_service_result() {
    UartControllerFixture fixture;

    fixture.controller.handleInstruction({ByteCode(ByteCodeEnum::Read, 1)});

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.lastBytecodes.size());
    TEST_ASSERT_EQUAL_UINT32(0, fixture.uartService.clearCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("UART Read: No data"));
}

void test_write_sends_decoded_text_payload_after_configuration() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.handleCommand(TerminalCommand("write", "hello\\n"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(6, fixture.uartService.writes.size());
    TEST_ASSERT_TRUE(fixture.uartService.wrote("h"));
    TEST_ASSERT_TRUE(fixture.uartService.wrote("\n"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Write: sent 6 bytes"));
}

void test_write_sends_hex_payload_and_rejects_wildcards() {
    UartControllerFixture sent;
    queueDefaultConfiguration(sent);
    sent.controller.handleCommand(TerminalCommand("write", "hex{ AA 10 }"));

    TEST_ASSERT_EQUAL_UINT32(2, sent.uartService.writes.size());
    TEST_ASSERT_TRUE(sent.uartService.wrote(std::string(1, static_cast<char>(0xAA))));
    TEST_ASSERT_TRUE(sent.uartService.wrote(std::string(1, static_cast<char>(0x10))));

    UartControllerFixture rejected;
    queueDefaultConfiguration(rejected);
    rejected.controller.handleCommand(TerminalCommand("write", "hex{ AA ?? }"));

    TEST_ASSERT_TRUE(rejected.view.contains("wildcards"));
    TEST_ASSERT_TRUE(rejected.uartService.writes.empty());
}

void test_read_streams_uart_data_until_enter() {
    UartControllerFixture fixture;
    fixture.uartService.queueRx("OK");
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("read"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.flushCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("OK"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Read: Stopped by user."));
}

void test_raw_read_formats_pending_row_on_enter() {
    UartControllerFixture fixture;
    fixture.uartService.queueRx(std::string("\x01""A", 2));
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("raw"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.flushCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("01 41"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Read (raw): Stopped by user."));
}

void test_autobaud_detects_and_saves_baudrate() {
    UartControllerFixture fixture;
    fixture.uartService.detectBaudResults.push_back(115200);
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueLine("y");

    fixture.controller.handleCommand(TerminalCommand("autobaud"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.clearCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.detectBaudPins.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.uartService.detectBaudPins[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.switchedBauds.size());
    TEST_ASSERT_EQUAL_UINT32(115200, fixture.uartService.switchedBauds[0]);
    TEST_ASSERT_EQUAL_UINT32(115200, GlobalState::getInstance().getUartBaudRate());
    TEST_ASSERT_TRUE(fixture.view.contains("UART Autobaud: Baudrate detected 115200"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Autobaud: Baudrate saved to config."));
}

void test_autobaud_can_stop_before_detection() {
    UartControllerFixture fixture;
    fixture.terminalInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("autobaud"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.clearCalls);
    TEST_ASSERT_TRUE(fixture.uartService.detectBaudPins.empty());
    TEST_ASSERT_TRUE(fixture.uartService.switchedBauds.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("UART Autobaud: Stopped by user."));
}

void test_scan_reports_active_pins_and_updates_device_view() {
    UartControllerFixture fixture;
    GlobalState::getInstance().setJtagScanPins({6, 7});
    fixture.terminalInput.queueLine("");
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar('\n');
    fixture.utility.queueNowMs(0);
    fixture.utility.queueNowMs(1000);
    fixture.uartService.scanResults.push_back({
        UartPinActivity{6, 12, 400.0f, 9600},
        UartPinActivity{7, 8, 266.0f, 19200}
    });

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.scanPinsCalls.size());
    TEST_ASSERT_EQUAL_UINT32(2, fixture.uartService.scanPinsCalls[0].size());
    TEST_ASSERT_EQUAL_UINT8(6, fixture.uartService.scanPinsCalls[0][0]);
    TEST_ASSERT_EQUAL_UINT8(7, fixture.uartService.scanPinsCalls[0][1]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.device.shownConfigs.size());
    TEST_ASSERT_TRUE(fixture.view.contains("GPIO 6 | edges=12 | approxBaud=9600"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Scan: Stopped by user."));
}

void test_bridge_forwards_terminal_data_and_displays_uart_data() {
    UartControllerFixture fixture;
    fixture.uartService.queueRx("R");
    fixture.terminalInput.queueReadChar('x');
    fixture.deviceInput.queueReadChar(KEY_NONE);
    fixture.deviceInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("bridge"));

    TEST_ASSERT_TRUE(fixture.view.contains("R"));
    TEST_ASSERT_TRUE(fixture.uartService.wrote("x"));
    TEST_ASSERT_TRUE(fixture.view.contains("Uart Bridge: Stopped by user."));
}

void test_spam_sends_payload_on_elapsed_delay_then_stops() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);
    fixture.utility.queueNowMs(10);
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("spam", "hi", "10"));

    TEST_ASSERT_TRUE(fixture.uartService.wrote("h"));
    TEST_ASSERT_TRUE(fixture.uartService.wrote("i"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Spam: Sending 2 bytes every 10 ms"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Spam: Stopped by user."));
}

void test_spam_rejects_hex_wildcards_without_writing() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);

    fixture.controller.handleCommand(TerminalCommand("spam", "hex{", "AA ?? } 10"));

    TEST_ASSERT_TRUE(fixture.uartService.writes.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("UART Spam: wildcards are not allowed"));
}

void test_trigger_sends_response_when_text_pattern_matches() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);
    fixture.uartService.queueRx("prefix Hit ESC key suffix");
    fixture.utility.queueNowMs(20);
    fixture.terminalInput.queueLine("\\x1B");
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("trigger", "Hit", "ESC key"));

    TEST_ASSERT_TRUE(fixture.uartService.wrote(std::string(1, '\x1B')));
    TEST_ASSERT_TRUE(fixture.view.contains("[TRIGGER] match -> sent \\x1B"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Trigger: Stopped by user."));
}

void test_trigger_rejects_invalid_pattern_without_listening() {
    UartControllerFixture fixture;
    queueDefaultConfiguration(fixture);
    fixture.terminalInput.queueLine("\\x1B");

    fixture.controller.handleCommand(TerminalCommand("trigger", "hex{", "GG }"));

    TEST_ASSERT_TRUE(fixture.uartService.writes.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("UART Trigger: Invalid pattern."));
}

void test_at_and_emulator_delegate_to_shell_interfaces() {
    UartControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("at"));
    fixture.controller.handleCommand(TerminalCommand("emulator"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.atShell.runCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.emulationShell.runCalls);
}

void test_swap_exchanges_pins_and_reconfigures_uart() {
    UartControllerFixture fixture;
    auto& state = GlobalState::getInstance();
    state.setUartRxPin(8);
    state.setUartTxPin(9);
    state.setUartBaudRate(115200);
    state.setUartConfig(0xABCDEF01);
    state.setUartInverted(true);

    fixture.controller.handleCommand(TerminalCommand("swap"));

    TEST_ASSERT_EQUAL_UINT8(9, state.getUartRxPin());
    TEST_ASSERT_EQUAL_UINT8(8, state.getUartTxPin());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.configurations.size());
    const auto& config = fixture.uartService.configurations[0];
    TEST_ASSERT_EQUAL_UINT32(115200, config.baud);
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF01, config.config);
    TEST_ASSERT_EQUAL_UINT8(9, config.rx);
    TEST_ASSERT_EQUAL_UINT8(8, config.tx);
    TEST_ASSERT_TRUE(config.inverted);
}

void test_sniff_routes_text_and_raw_to_sniffer_service() {
    UartControllerFixture text;
    GlobalState::getInstance().setUartRxPin(8);
    GlobalState::getInstance().setUartTxPin(9);
    GlobalState::getInstance().setUartBaudRate(57600);
    GlobalState::getInstance().setUartConfig(0x11223344);
    GlobalState::getInstance().setUartInverted(true);
    queueDefaultConfiguration(text);

    text.controller.handleCommand(TerminalCommand("sniff", "txt"));

    TEST_ASSERT_EQUAL_UINT32(1, text.uartSnifferService.textCalls);
    TEST_ASSERT_EQUAL_UINT32(0, text.uartSnifferService.rawCalls);
    TEST_ASSERT_EQUAL_UINT32(57600, text.uartSnifferService.lastText.baud);
    TEST_ASSERT_EQUAL_HEX32(0x11223344, text.uartSnifferService.lastText.config);
    TEST_ASSERT_TRUE(text.uartSnifferService.lastText.inverted);
    TEST_ASSERT_EQUAL_UINT8(8, text.uartSnifferService.lastText.rxPin1);
    TEST_ASSERT_EQUAL_UINT8(9, text.uartSnifferService.lastText.rxPin2);

    UartControllerFixture raw;
    GlobalState::getInstance().setUartRxPin(10);
    GlobalState::getInstance().setUartTxPin(11);
    queueDefaultConfiguration(raw);

    raw.controller.handleCommand(TerminalCommand("sniff", "raw"));

    TEST_ASSERT_EQUAL_UINT32(1, raw.uartSnifferService.rawCalls);
    TEST_ASSERT_EQUAL_UINT8(10, raw.uartSnifferService.lastRaw.rxPin1);
    TEST_ASSERT_EQUAL_UINT8(11, raw.uartSnifferService.lastRaw.rxPin2);
}

void test_sniff_rejects_identical_pins_without_running_sniffer() {
    UartControllerFixture fixture;
    GlobalState::getInstance().setUartRxPin(8);
    GlobalState::getInstance().setUartTxPin(8);

    fixture.controller.handleCommand(TerminalCommand("sniff", "raw"));

    TEST_ASSERT_EQUAL_UINT32(0, fixture.uartSnifferService.rawCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("RX and TX pins are identical"));
}

void test_ping_reports_detected_ascii_response() {
    UartControllerFixture fixture;
    fixture.utility.advanceTimeOnSleep = true;
    fixture.uartService.queueRx("READY\r\n");

    fixture.controller.handleCommand(TerminalCommand("ping"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.uartService.clearCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("UART Response"));
    TEST_ASSERT_TRUE(fixture.view.contains("READY"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Ping: Device detected"));
}

void test_xmodem_reports_usage_errors_before_touching_sd() {
    UartControllerFixture missingAction;
    missingAction.controller.handleCommand(TerminalCommand("xmodem"));
    TEST_ASSERT_TRUE(missingAction.view.contains("Usage: xmodem <recv/send> <path>"));

    UartControllerFixture missingPath;
    missingPath.controller.handleCommand(TerminalCommand("xmodem", "recv"));
    TEST_ASSERT_TRUE(missingPath.view.contains("Error: missing path argument"));
}

void test_glitch_reports_not_implemented() {
    UartControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("glitch"));

    TEST_ASSERT_TRUE(fixture.view.contains("Uart Glicher: Not Yet Implemented"));
}

void test_unknown_command_displays_uart_help() {
    UartControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("unknown"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available UART commands"));
    TEST_ASSERT_TRUE(fixture.view.contains("Full-duplex mode"));
}

}  // namespace uart_controller_tests

void runUartControllerTests() {
    using namespace uart_controller_tests;
    RUN_TEST(test_config_applies_selected_serial_parameters);
    RUN_TEST(test_ensure_configured_configures_once_and_skips_redundant_reinit);
    RUN_TEST(test_ensure_configured_reapplies_when_saved_state_changes);
    RUN_TEST(test_on_mode_enter_reinitializes_uart_unconditionally);
    RUN_TEST(test_uread_reports_hex_bytes_within_timeout);
    RUN_TEST(test_uread_reports_none_when_bus_idle);
    RUN_TEST(test_instruction_delegates_bytecodes_and_clears_buffer_when_data_is_read);
    RUN_TEST(test_instruction_reports_no_data_for_empty_service_result);
    RUN_TEST(test_write_sends_decoded_text_payload_after_configuration);
    RUN_TEST(test_write_sends_hex_payload_and_rejects_wildcards);
    RUN_TEST(test_read_streams_uart_data_until_enter);
    RUN_TEST(test_raw_read_formats_pending_row_on_enter);
    RUN_TEST(test_autobaud_detects_and_saves_baudrate);
    RUN_TEST(test_autobaud_can_stop_before_detection);
    RUN_TEST(test_scan_reports_active_pins_and_updates_device_view);
    RUN_TEST(test_bridge_forwards_terminal_data_and_displays_uart_data);
    RUN_TEST(test_spam_sends_payload_on_elapsed_delay_then_stops);
    RUN_TEST(test_spam_rejects_hex_wildcards_without_writing);
    RUN_TEST(test_trigger_sends_response_when_text_pattern_matches);
    RUN_TEST(test_trigger_rejects_invalid_pattern_without_listening);
    RUN_TEST(test_at_and_emulator_delegate_to_shell_interfaces);
    RUN_TEST(test_swap_exchanges_pins_and_reconfigures_uart);
    RUN_TEST(test_sniff_routes_text_and_raw_to_sniffer_service);
    RUN_TEST(test_sniff_rejects_identical_pins_without_running_sniffer);
    RUN_TEST(test_ping_reports_detected_ascii_response);
    RUN_TEST(test_xmodem_reports_usage_errors_before_touching_sd);
    RUN_TEST(test_glitch_reports_not_implemented);
    RUN_TEST(test_unknown_command_displays_uart_help);
}
