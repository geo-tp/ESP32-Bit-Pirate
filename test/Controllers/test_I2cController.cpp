#include <unity.h>

#include "Controllers/I2cController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeI2cService.h"
#include "../Services/FakeUtilityService.h"
#include "../Shells/FakeI2cEepromShell.h"
#include "../Services/FakeI2cSnifferService.h"
#include "../Views/FakeTerminalView.h"

namespace i2c_controller_tests {

struct I2cControllerFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeUtilityService utility;
    FakeI2cService i2cService;
    FakeI2cSnifferService snifferService;
    FakeI2cEepromShell eepromShell;
    ArgTransformer transformer;
    UserInputManager userInput{view, input, transformer};
    HelpShell helpShell{view, input, userInput};
    I2cController controller{
        view,
        input,
        utility,
        i2cService,
        snifferService,
        transformer,
        userInput,
        eepromShell,
        helpShell
    };

    I2cControllerFixture() {
        auto& state = GlobalState::getInstance();
        state.setCurrentMode(ModeEnum::I2C);
        state.setI2cSdaPin(2);
        state.setI2cSclPin(1);
        state.setI2cFrequency(100000);
        resetFakeI2cSniffer();
    }

    void configureOnce() {
        input.queueLine("");
        input.queueLine("");
        input.queueLine("");
        controller.ensureConfigured();
    }
};

void test_instruction_delegates_bytecodes_and_displays_non_empty_result() {
    I2cControllerFixture fixture;
    fixture.i2cService.byteCodeResult = "DE AD";
    const std::vector<ByteCode> bytecodes = {
        ByteCode(ByteCodeEnum::Start),
        ByteCode(ByteCodeEnum::Write, 0xA0),
        ByteCode(ByteCodeEnum::Stop)
    };

    fixture.controller.handleInstruction(bytecodes);

    TEST_ASSERT_EQUAL_UINT32(3, fixture.i2cService.lastBytecodes.size());
    TEST_ASSERT_EQUAL_HEX32(0xA0, fixture.i2cService.lastBytecodes[1].getData());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Read:"));
    TEST_ASSERT_TRUE(fixture.view.contains("DE AD"));
}

void test_instruction_keeps_terminal_quiet_for_empty_result() {
    I2cControllerFixture fixture;

    fixture.controller.handleInstruction({ByteCode(ByteCodeEnum::Write, 0x55)});

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.lastBytecodes.size());
    TEST_ASSERT_TRUE(fixture.view.output.empty());
}

void test_ping_sends_address_and_reports_ack() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("ping", "0x3C"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_EQUAL_HEX8(0x3C, fixture.i2cService.beginAddresses[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("ACK received"));
}

void test_ping_reports_nack() {
    I2cControllerFixture fixture;
    fixture.i2cService.endTransmissionResults.push_back(true);

    fixture.controller.handleCommand(TerminalCommand("ping", "60"));

    TEST_ASSERT_EQUAL_HEX8(0x3C, fixture.i2cService.beginAddresses[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("No response"));
}

void test_ping_rejects_partial_and_out_of_range_addresses() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("ping", "12junk"));
    fixture.controller.handleCommand(TerminalCommand("ping", "0x80"));
    fixture.controller.handleCommand(TerminalCommand("ping", "18446744073709551616"));

    TEST_ASSERT_TRUE(fixture.i2cService.beginAddresses.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid address"));
}

void test_scan_probes_all_seven_bit_addresses_and_reports_acks() {
    I2cControllerFixture fixture;
    fixture.i2cService.useAddressAcknowledgements = true;
    fixture.i2cService.acknowledgedAddresses = {0x20, 0x50};
    for (int i = 0; i < 126; ++i) fixture.input.queueReadChar(KEY_NONE);

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_EQUAL_UINT32(126, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.beginAddresses.front());
    TEST_ASSERT_EQUAL_UINT8(126, fixture.i2cService.beginAddresses.back());
    TEST_ASSERT_TRUE(fixture.view.contains("Found device at 0x20"));
    TEST_ASSERT_TRUE(fixture.view.contains("Found device at 0x50"));
}

void test_scan_can_be_cancelled_before_touching_the_bus() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_TRUE(fixture.i2cService.beginAddresses.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Cancelled by user"));
}

void test_write_confirms_then_writes_register_and_value() {
    I2cControllerFixture fixture;
    fixture.input.queueLine("");

    fixture.controller.handleCommand(TerminalCommand("write", "0x50", "0x10 0xAB"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_EQUAL_HEX8(0x50, fixture.i2cService.beginAddresses[0]);
    TEST_ASSERT_EQUAL_HEX8(0x50, fixture.i2cService.beginAddresses[1]);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.writtenBytes.size());
    TEST_ASSERT_EQUAL_HEX8(0x10, fixture.i2cService.writtenBytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAB, fixture.i2cService.writtenBytes[1]);
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Write: 0xAB"));
}

void test_write_cancelled_after_ping_does_not_modify_register() {
    I2cControllerFixture fixture;
    fixture.input.queueLine("n");

    fixture.controller.handleCommand(TerminalCommand("write", "0x50", "1 2"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_TRUE(fixture.i2cService.writtenBytes.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Write: Cancelled"));
}

void test_write_rejects_address_that_would_be_truncated_to_eight_bits() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("write", "0x100", "1 2"));

    TEST_ASSERT_TRUE(fixture.i2cService.beginAddresses.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid address"));
}

void test_read_uses_repeated_start_and_displays_register_value() {
    I2cControllerFixture fixture;
    fixture.i2cService.rxData.push_back(0xAB);

    fixture.controller.handleCommand(TerminalCommand("read", "0x50", "0x10"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.writtenBytes.size());
    TEST_ASSERT_EQUAL_HEX8(0x10, fixture.i2cService.writtenBytes[0]);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.endTransmissionStops.size());
    TEST_ASSERT_TRUE(fixture.i2cService.endTransmissionStops[0]);
    TEST_ASSERT_FALSE(fixture.i2cService.endTransmissionStops[1]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.requests.size());
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.requests[0].quantity);
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Read: 0xAB (171)"));
}

void test_read_stops_when_device_does_not_ack() {
    I2cControllerFixture fixture;
    fixture.i2cService.endTransmissionResults.push_back(true);

    fixture.controller.handleCommand(TerminalCommand("read", "0x50", "0"));

    TEST_ASSERT_TRUE(fixture.i2cService.requests.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("No device found at 0x50"));
}

void test_dump_reads_burst_and_formats_hex_and_ascii() {
    I2cControllerFixture fixture;
    fixture.i2cService.rxData = {0x41, 0x42, 0x00, 0x7F};
    for (int i = 0; i < 4; ++i) fixture.input.queueReadChar(KEY_NONE);

    fixture.controller.handleCommand(TerminalCommand("dump", "0x50", "4"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.requests.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.i2cService.requests[0].quantity);
    TEST_ASSERT_TRUE(fixture.view.contains("00: 41 42 00 7F"));
    TEST_ASSERT_TRUE(fixture.view.contains("AB.."));
}

void test_dump_completes_register_scan_then_uses_direct_raw_probe() {
    I2cControllerFixture fixture;
    fixture.i2cService.requestResults = {0, 0, 0};
    fixture.input.queueReadChar(KEY_NONE);

    fixture.controller.handleCommand(TerminalCommand("dump", "0x50"));

    // The register scan is not abandoned when the first offset yields nothing.
    // A device may reject register 0 and still expose valid registers or
    // commands at other offsets, so every offset is probed before falling back.
    TEST_ASSERT_EQUAL_UINT32(33, fixture.i2cService.requests.size());
    TEST_ASSERT_EQUAL_UINT8(16, fixture.i2cService.requests[0].quantity);
    // The raw fallback then probes a single byte without writing a register
    // pointer, so devices with a non-register protocol still answer.
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.requests.back().quantity);
    TEST_ASSERT_TRUE(fixture.view.contains("unsupported protocol"));
}

void test_dump_continues_sequential_raw_reads_after_probe_succeeds() {
    I2cControllerFixture fixture;
    fixture.i2cService.endTransmissionResults = {false, true};
    fixture.i2cService.requestResults = {1, 3};
    fixture.i2cService.rxData = {0x41, 0x42, 0x43, 0x44};
    for (int i = 0; i < 2; ++i) fixture.input.queueReadChar(KEY_NONE);

    fixture.controller.handleCommand(TerminalCommand("dump", "0x50", "4"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.requests.size());
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.requests[0].quantity);
    TEST_ASSERT_EQUAL_UINT8(3, fixture.i2cService.requests[1].quantity);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.writtenBytes.size());
    TEST_ASSERT_TRUE(fixture.view.contains("00: 41 42 43 44"));
    TEST_ASSERT_TRUE(fixture.view.contains("ABCD"));
}

void test_dump_can_cancel_after_successful_raw_probe() {
    I2cControllerFixture fixture;
    fixture.i2cService.endTransmissionResults = {false, true};
    fixture.i2cService.requestResults = {1};
    fixture.i2cService.rxData = {0x41};
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("dump", "0x50", "4"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.requests.size());
    TEST_ASSERT_TRUE(fixture.view.contains("Cancelled by user"));
    TEST_ASSERT_TRUE(fixture.view.contains("00: 41 ?? ?? ??"));
}

void test_config_updates_state_and_configures_service() {
    I2cControllerFixture fixture;
    fixture.input.queueLine("4");
    fixture.input.queueLine("5");
    fixture.input.queueLine("400000");

    fixture.controller.handleCommand(TerminalCommand("config"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    const auto& config = fixture.i2cService.configurations[0];
    TEST_ASSERT_EQUAL_UINT8(4, config.sda);
    TEST_ASSERT_EQUAL_UINT8(5, config.scl);
    TEST_ASSERT_EQUAL_UINT32(400000, config.frequency);
    TEST_ASSERT_EQUAL_UINT8(4, GlobalState::getInstance().getI2cSdaPin());
    TEST_ASSERT_EQUAL_UINT8(5, GlobalState::getInstance().getI2cSclPin());
}

void test_ensure_configured_prompts_once_then_reapplies_saved_state() {
    I2cControllerFixture fixture;
    fixture.configureOnce();

    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
    TEST_ASSERT_EQUAL_UINT8(2, fixture.i2cService.configurations[1].sda);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.configurations[1].scl);
}

void test_swap_updates_state_and_reconfigures_bus() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("swap"));

    TEST_ASSERT_EQUAL_UINT8(1, GlobalState::getInstance().getI2cSdaPin());
    TEST_ASSERT_EQUAL_UINT8(2, GlobalState::getInstance().getI2cSclPin());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.configurations[0].sda);
    TEST_ASSERT_EQUAL_UINT8(2, fixture.i2cService.configurations[0].scl);
}

void test_recover_releases_bitbangs_and_reconfigures_bus() {
    I2cControllerFixture fixture;
    fixture.i2cService.recoverResult = true;

    fixture.controller.handleCommand(TerminalCommand("recover"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.recoverCalls);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.recoverScl);
    TEST_ASSERT_EQUAL_UINT8(2, fixture.i2cService.recoverSda);
    TEST_ASSERT_EQUAL_UINT32(100000, fixture.i2cService.recoverFrequency);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_TRUE(fixture.view.contains("Bus recovery successful"));
}

void test_slave_starts_and_stops_service_then_restores_master_bus() {
    I2cControllerFixture fixture;
    fixture.configureOnce();
    fixture.i2cService.configurations.clear();

    fixture.controller.handleCommand(TerminalCommand("slave", "0x42"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.beginSlaveCalls);
    TEST_ASSERT_EQUAL_HEX8(0x42, fixture.i2cService.slaveAddress);
    TEST_ASSERT_EQUAL_UINT8(2, fixture.i2cService.slaveSda);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.slaveScl);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endSlaveCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.clearSlaveLogCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Slave: Stopped by user"));
}

void test_glitch_runs_complete_attack_sequence_then_restores_bus() {
    I2cControllerFixture fixture;
    fixture.configureOnce();
    fixture.i2cService.configurations.clear();

    fixture.controller.handleCommand(TerminalCommand("glitch", "0x50"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.floodRandomCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.floodStartCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.overReadAttackCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.invalidRegisterReadCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.simulateClockStretchCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.rapidStartStopCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.glitchAckInjectionCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.randomClockPulseNoiseCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
}

void test_flood_writes_random_register_until_user_stops() {
    I2cControllerFixture fixture;
    fixture.utility.randomValue = 0x15A;
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("flood", "0x50"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.beginAddresses.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.writtenBytes.size());
    TEST_ASSERT_EQUAL_HEX8(0x5A, fixture.i2cService.writtenBytes[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("Stopped by user"));
}

void test_jam_injects_noise_recovers_and_restores_configured_bus() {
    I2cControllerFixture fixture;
    fixture.configureOnce();
    fixture.i2cService.configurations.clear();
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.controller.handleCommand(TerminalCommand("jam"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.injectRandomGlitchCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.recoverCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Jam: Stopped by user"));
}

void test_trace_reports_register_change_and_stops_during_delay() {
    I2cControllerFixture fixture;
    fixture.i2cService.readRegResults.push_back({true, 0x10});
    fixture.i2cService.readRegResults.push_back({true, 0x11});

    fixture.controller.handleCommand(TerminalCommand("trace", "0x50", "2 100"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.readRegCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("0x02: 0x10"));
    TEST_ASSERT_TRUE(fixture.view.contains("-> 0x11"));
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Trace: Stopped by user"));
}

void test_health_computes_stable_ping_and_register_metrics() {
    I2cControllerFixture fixture;
    fixture.i2cService.pingResult = true;
    fixture.i2cService.pingDtUs = 12;
    fixture.i2cService.readRegResult = true;
    fixture.i2cService.readRegDtUs = 20;
    for (int i = 0; i < 99; ++i) fixture.input.queueReadChar(KEY_NONE);

    fixture.controller.handleCommand(TerminalCommand("health", "0x50"));

    TEST_ASSERT_EQUAL_UINT32(51, fixture.i2cService.pingCalls);
    TEST_ASSERT_EQUAL_UINT32(50, fixture.i2cService.readRegCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("✅ Stable ACK"));
    TEST_ASSERT_TRUE(fixture.view.contains("✅ Stable reads"));
}

void test_regs_requires_explicit_confirmation_before_write_probe() {
    I2cControllerFixture fixture;
    fixture.input.queueLine("n");

    fixture.controller.handleCommand(TerminalCommand("regs", "0x50", "4"));

    TEST_ASSERT_TRUE(fixture.view.contains("I2C Regs: Stopped by user"));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endTransmissionCalls);
}

void test_eeprom_delegates_valid_address_and_restores_i2c_config() {
    I2cControllerFixture fixture;
    fixture.configureOnce();
    fixture.i2cService.configurations.clear();

    fixture.controller.handleCommand(TerminalCommand("eeprom", "0x51"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.eepromShell.addresses.size());
    TEST_ASSERT_EQUAL_HEX8(0x51, fixture.eepromShell.addresses[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
}

void test_eeprom_rejects_reserved_or_overflowing_address() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("eeprom", "0x02"));
    fixture.controller.handleCommand(TerminalCommand("eeprom", "0x150"));

    TEST_ASSERT_TRUE(fixture.eepromShell.addresses.empty());
}

void test_sniff_uses_current_pins_then_reconfigures_service() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("sniff"));

    TEST_ASSERT_EQUAL_UINT32(1, fakeI2cSnifferBeginCalls);
    TEST_ASSERT_EQUAL_UINT8(1, fakeI2cSnifferScl);
    TEST_ASSERT_EQUAL_UINT8(2, fakeI2cSnifferSda);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Sniffer: Stopped"));
}

void test_sniff_applies_address_filter_then_clears_it() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("sniff", "0x53"));

    TEST_ASSERT_EQUAL_UINT32(2, fakeI2cSnifferFilterCalls);
    TEST_ASSERT_EQUAL_HEX8(0x53, fakeI2cSnifferLastEnabledAddress);
    TEST_ASSERT_FALSE(fakeI2cSnifferFilterEnabled);
    TEST_ASSERT_EQUAL_HEX8(0x00, fakeI2cSnifferFilterAddress);
    TEST_ASSERT_TRUE(fixture.view.contains("Listening for address 0x53"));
}

void test_frequency_waits_without_timeout_and_restores_bus_without_changing_speed() {
    I2cControllerFixture fixture;
    fixture.snifferService.frequencyResult = {true, 30, 27, 400000, 2.5, 1.5, 1.0};
    fixture.snifferService.onFrequencyMeasure = [&fixture]() {
        TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
        TEST_ASSERT_TRUE(fixture.i2cService.configurations.empty());
    };

    fixture.controller.handleCommand(TerminalCommand("freq"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.snifferService.frequencyCalls);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.snifferService.frequencyScl);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.snifferService.frequencyTimeoutMs);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(2, fixture.i2cService.configurations[0].sda);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.i2cService.configurations[0].scl);
    TEST_ASSERT_EQUAL_UINT32(100000, fixture.i2cService.configurations[0].frequency);
    TEST_ASSERT_EQUAL_UINT32(100000, GlobalState::getInstance().getI2cFrequency());
    TEST_ASSERT_TRUE(fixture.i2cService.beginAddresses.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("I2C Freq: Listening"));
    TEST_ASSERT_TRUE(fixture.view.contains("[ENTER] to stop"));
    TEST_ASSERT_TRUE(fixture.view.contains("Freq: 400.00 kHz"));
    TEST_ASSERT_TRUE(fixture.view.contains("LOW: 1.50 us"));
    TEST_ASSERT_TRUE(fixture.view.contains("HIGH: 1.00 us"));
    TEST_ASSERT_TRUE(fixture.view.contains("Cycles: 27/30"));
    TEST_ASSERT_TRUE(fixture.view.contains("Save frequency to the configuration? [n]"));
    TEST_ASSERT_FALSE(fixture.view.contains("Frequency saved"));
}

void test_frequency_saves_detected_speed_only_after_confirmation() {
    for (bool confirm : {false, true}) {
        I2cControllerFixture fixture;
        fixture.snifferService.frequencyResult = {true, 30, 27, 400000, 2.5, 1.5, 1.0};
        fixture.input.queueLine(confirm ? "y" : "n");

        fixture.controller.handleCommand(TerminalCommand("freq"));

        TEST_ASSERT_EQUAL_UINT32(confirm ? 400000 : 100000,
                                GlobalState::getInstance().getI2cFrequency());
        TEST_ASSERT_EQUAL(confirm, fixture.view.contains("Frequency saved to configuration."));
    }
}

void test_frequency_reports_unreliable_capture() {
    I2cControllerFixture fixture;
    fixture.controller.handleCommand(TerminalCommand("freq"));
    TEST_ASSERT_TRUE(fixture.view.contains("Freq: No stable clock."));
    TEST_ASSERT_FALSE(fixture.view.contains("kHz"));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
}

void test_frequency_failure_and_enter_cancellation_restore_bus() {
    for (bool fail : {true, false}) {
        I2cControllerFixture fixture;
        fixture.snifferService.frequencySuccess = !fail;
        fixture.snifferService.pollFrequencyCancellation = !fail;
        fixture.input.queueReadChar('\n');
        fixture.controller.handleCommand(TerminalCommand("freq"));
        TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.configurations.size());
        TEST_ASSERT_TRUE(fixture.view.contains(fail ? "Freq error:" : "Stopped by user."));
        TEST_ASSERT_EQUAL(!fail, fixture.snifferService.frequencyCancelled);
        TEST_ASSERT_FALSE(fixture.view.contains("kHz"));
        TEST_ASSERT_FALSE(fixture.view.contains("Save frequency"));
    }
}

void test_release_frees_sniffer_and_forces_next_configuration_prompt() {
    I2cControllerFixture fixture;
    fixture.configureOnce();
    fixture.controller.ensureReleased();
    fixture.input.queueLine("4");
    fixture.input.queueLine("5");
    fixture.input.queueLine("400000");

    fixture.controller.ensureConfigured();

    TEST_ASSERT_EQUAL_UINT32(1, fakeI2cSnifferReleaseCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.i2cService.endCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.i2cService.configurations.size());
    TEST_ASSERT_EQUAL_UINT8(4, fixture.i2cService.configurations[1].sda);
}

void test_identify_uses_known_address_database() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("identify", "0x3C"));

    TEST_ASSERT_TRUE(fixture.view.contains("SSD1306 OLED"));
    TEST_ASSERT_TRUE(fixture.view.contains("Identification Result"));
}

void test_unknown_command_displays_i2c_help() {
    I2cControllerFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("unknown"));

    TEST_ASSERT_TRUE(fixture.view.contains("Unknown command. Available I2C commands"));
    TEST_ASSERT_TRUE(fixture.view.contains("Find devices"));
}

}  // namespace i2c_controller_tests

void runI2cControllerTests() {
    using namespace i2c_controller_tests;
    RUN_TEST(test_instruction_delegates_bytecodes_and_displays_non_empty_result);
    RUN_TEST(test_instruction_keeps_terminal_quiet_for_empty_result);
    RUN_TEST(test_ping_sends_address_and_reports_ack);
    RUN_TEST(test_ping_reports_nack);
    RUN_TEST(test_ping_rejects_partial_and_out_of_range_addresses);
    RUN_TEST(test_scan_probes_all_seven_bit_addresses_and_reports_acks);
    RUN_TEST(test_scan_can_be_cancelled_before_touching_the_bus);
    RUN_TEST(test_write_confirms_then_writes_register_and_value);
    RUN_TEST(test_write_cancelled_after_ping_does_not_modify_register);
    RUN_TEST(test_write_rejects_address_that_would_be_truncated_to_eight_bits);
    RUN_TEST(test_read_uses_repeated_start_and_displays_register_value);
    RUN_TEST(test_read_stops_when_device_does_not_ack);
    RUN_TEST(test_dump_reads_burst_and_formats_hex_and_ascii);
    RUN_TEST(test_dump_completes_register_scan_then_uses_direct_raw_probe);
    RUN_TEST(test_dump_continues_sequential_raw_reads_after_probe_succeeds);
    RUN_TEST(test_dump_can_cancel_after_successful_raw_probe);
    RUN_TEST(test_config_updates_state_and_configures_service);
    RUN_TEST(test_ensure_configured_prompts_once_then_reapplies_saved_state);
    RUN_TEST(test_swap_updates_state_and_reconfigures_bus);
    RUN_TEST(test_recover_releases_bitbangs_and_reconfigures_bus);
    RUN_TEST(test_slave_starts_and_stops_service_then_restores_master_bus);
    RUN_TEST(test_glitch_runs_complete_attack_sequence_then_restores_bus);
    RUN_TEST(test_flood_writes_random_register_until_user_stops);
    RUN_TEST(test_jam_injects_noise_recovers_and_restores_configured_bus);
    RUN_TEST(test_trace_reports_register_change_and_stops_during_delay);
    RUN_TEST(test_health_computes_stable_ping_and_register_metrics);
    RUN_TEST(test_regs_requires_explicit_confirmation_before_write_probe);
    RUN_TEST(test_eeprom_delegates_valid_address_and_restores_i2c_config);
    RUN_TEST(test_eeprom_rejects_reserved_or_overflowing_address);
    RUN_TEST(test_sniff_uses_current_pins_then_reconfigures_service);
    RUN_TEST(test_sniff_applies_address_filter_then_clears_it);
    RUN_TEST(test_frequency_waits_without_timeout_and_restores_bus_without_changing_speed);
    RUN_TEST(test_frequency_saves_detected_speed_only_after_confirmation);
    RUN_TEST(test_frequency_reports_unreliable_capture);
    RUN_TEST(test_frequency_failure_and_enter_cancellation_restore_bus);
    RUN_TEST(test_release_frees_sniffer_and_forces_next_configuration_prompt);
    RUN_TEST(test_identify_uses_known_address_database);
    RUN_TEST(test_unknown_command_displays_i2c_help);
}
