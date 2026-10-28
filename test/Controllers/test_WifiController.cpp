#include <unity.h>

#include "Controllers/WifiController.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeEthernetService.h"
#include "../Services/FakeHttpService.h"
#include "../Services/FakeIcmpService.h"
#include "../Services/FakeNetcatService.h"
#include "../Services/FakeNmapService.h"
#include "../Services/FakeNvsService.h"
#include "../Services/FakeSshService.h"
#include "../Services/FakeTelnetService.h"
#include "../Services/FakeUtilityService.h"
#include "../Services/FakeWifiOpenScannerService.h"
#include "../Services/FakeWifiService.h"
#include "../Shells/FakeModbusShell.h"
#include "../Transformers/FakeJsonTransformer.h"
#include "../Views/FakeDeviceView.h"
#include "../Views/FakeTerminalView.h"

namespace wifi_controller_tests {

struct WifiFixture {
    FakeTerminalView view;
    FakeDeviceView device;
    FakeInput terminalInput;
    FakeInput deviceInput;
    FakeUtilityService utility;
    FakeWifiService wifi;
    FakeWifiOpenScannerService wifiOpenScanner;
    FakeEthernetService ethernet;
    FakeSshService ssh;
    FakeNetcatService netcat;
    FakeNmapService nmap;
    FakeIcmpService icmp;
    FakeNvsService nvs;
    FakeHttpService http;
    FakeTelnetService telnet;
    ArgTransformer argTransformer;
    FakeJsonTransformer jsonTransformer;
    UserInputManager userInput{view, terminalInput, argTransformer};
    FakeModbusShell modbusShell;
    HelpShell helpShell{view, terminalInput, userInput};
    WifiController controller{
        view,
        device,
        terminalInput,
        deviceInput,
        utility,
        wifi,
        wifiOpenScanner,
        ethernet,
        ssh,
        netcat,
        nmap,
        icmp,
        nvs,
        http,
        telnet,
        argTransformer,
        jsonTransformer,
        userInput,
        modbusShell,
        helpShell
    };

    WifiFixture() {
        GlobalState::getInstance().setCurrentMode(ModeEnum::WiFi);
    }
};

void test_build_wifi_lines_reports_disconnected_mode() {
    WifiFixture fixture;
    fixture.wifi.modeRaw = IWifiService::kWifiModeAp;
    fixture.wifi.statusRaw = 6;

    const auto lines = fixture.controller.buildWiFiLines();

    TEST_ASSERT_EQUAL_UINT32(2, lines.size());
    TEST_ASSERT_EQUAL_STRING("MODE AP", lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI DISCONNECTED", lines[1].c_str());
}

void test_build_wifi_lines_reports_connected_ip_and_truncated_ssid() {
    WifiFixture fixture;
    fixture.wifi.statusRaw = IWifiService::kWifiStatusConnected;
    fixture.wifi.localIp = "192.168.1.50";
    fixture.wifi.ssid = "Very Long Laboratory Network";

    const auto lines = fixture.controller.buildWiFiLines();

    TEST_ASSERT_EQUAL_UINT32(4, lines.size());
    TEST_ASSERT_EQUAL_STRING("WIFI CONNECTED", lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING("Very Long Labor...", lines[3].c_str());
}

void test_connect_with_inline_credentials_saves_nvs_on_success() {
    WifiFixture fixture;
    fixture.wifi.connectResult = true;

    fixture.controller.handleCommand(TerminalCommand("connect", "LabNet", "secret"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.setModeApStaCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.connectCalls.size());
    TEST_ASSERT_EQUAL_STRING("LabNet", fixture.wifi.connectCalls[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", fixture.wifi.connectCalls[0].password.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, fixture.nvs.savedStrings.size());
    TEST_ASSERT_TRUE(fixture.view.contains("Connected successfully"));
}

void test_connect_failure_resets_station_when_not_hotspot_terminal() {
    WifiFixture fixture;
    fixture.wifi.connectResult = false;

    fixture.controller.handleCommand(TerminalCommand("connect", "LabNet", "badpass"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.connectCalls.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.resetCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Connection failed"));
}

void test_connect_without_args_uses_confirmed_saved_credentials() {
    WifiFixture fixture;
    fixture.nvs.values[GlobalState::getInstance().getNvsSsidField()] = "SavedNet";
    fixture.nvs.values[GlobalState::getInstance().getNvsPasswordField()] = "savedpass";
    fixture.wifi.connectResult = true;

    fixture.controller.handleCommand(TerminalCommand("connect"));

    TEST_ASSERT_EQUAL_UINT32(2, fixture.nvs.openCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.nvs.closeCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.connectCalls.size());
    TEST_ASSERT_EQUAL_STRING("SavedNet", fixture.wifi.connectCalls[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("savedpass", fixture.wifi.connectCalls[0].password.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("Connected successfully"));
}

void test_disconnect_delegates_to_service() {
    WifiFixture fixture;
    fixture.wifi.connected = true;

    fixture.controller.handleCommand(TerminalCommand("disconnect"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.disconnectCalls);
    TEST_ASSERT_FALSE(fixture.wifi.connected);
    TEST_ASSERT_TRUE(fixture.view.contains("Disconnected"));
}

void test_status_prints_station_details_and_connected_metrics() {
    WifiFixture fixture;
    fixture.wifi.statusRaw = IWifiService::kWifiStatusConnected;
    fixture.wifi.modeRaw = IWifiService::kWifiModeSta;
    fixture.wifi.hostname.clear();
    fixture.wifi.bssid.clear();
    fixture.wifi.rssi = -63;
    fixture.wifi.channel = 11;

    fixture.controller.handleCommand(TerminalCommand("status"));

    TEST_ASSERT_TRUE(fixture.view.contains("Mode         : Station"));
    TEST_ASSERT_TRUE(fixture.view.contains("Hostname     : N/A"));
    TEST_ASSERT_TRUE(fixture.view.contains("BSSID        : N/A"));
    TEST_ASSERT_TRUE(fixture.view.contains("RSSI         : -63 dBm"));
    TEST_ASSERT_TRUE(fixture.view.contains("Channel      : 11"));
}

void test_scan_formats_detailed_network_flags() {
    WifiFixture fixture;
    WiFiNetwork network;
    network.ssid = "OpenLab";
    network.encryption = 0;
    network.bssid = "AA:BB";
    network.channel = 11;
    network.rssi = -42;
    network.open = true;
    network.vulnerable = true;
    network.hidden = true;
    fixture.wifi.detailedNetworks.push_back(network);

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_TRUE(fixture.view.contains("SSID: OpenLab"));
    TEST_ASSERT_TRUE(fixture.view.contains("Sec: OPEN"));
    TEST_ASSERT_TRUE(fixture.view.contains("[open]"));
    TEST_ASSERT_TRUE(fixture.view.contains("[vulnerable]"));
    TEST_ASSERT_TRUE(fixture.view.contains("[hidden]"));
}

void test_scan_reports_empty_result() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("scan"));

    TEST_ASSERT_TRUE(fixture.view.contains("No networks found"));
}

void test_spoof_defaults_to_station_interface() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("spoof", "02:AA:BB:CC:DD:EE"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.spoofedMacs.size());
    TEST_ASSERT_EQUAL_STRING("02:AA:BB:CC:DD:EE", fixture.wifi.spoofedMacs[0].c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiMacInterface::Station), static_cast<int>(fixture.wifi.spoofedInterfaces[0]));
    TEST_ASSERT_TRUE(fixture.view.contains("MAC spoofed successfully"));
}

void test_spoof_rejects_invalid_interface_without_service_call() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("spoof", "bad", "02:AA:BB:CC:DD:EE"));

    TEST_ASSERT_TRUE(fixture.wifi.spoofedMacs.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid mode"));
}

void test_ap_start_uses_ap_only_mode_when_station_is_disconnected() {
    WifiFixture fixture;
    fixture.wifi.connected = false;

    fixture.controller.handleCommand(TerminalCommand("ap", "PirateAP", "supersecretpass"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.setModeApOnlyCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.accessPointCalls.size());
    TEST_ASSERT_EQUAL_STRING("PirateAP", fixture.wifi.accessPointCalls[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("supersecretpass", fixture.wifi.accessPointCalls[0].password.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("Access Point is started"));
}

void test_ap_stop_delegates_to_service() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("ap", "stop"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.stopAccessPointCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Access Point stopped"));
}

void test_ap_start_reports_service_failure() {
    WifiFixture fixture;
    fixture.wifi.startAccessPointResult = false;

    fixture.controller.handleCommand(TerminalCommand("ap", "PirateAP", "supersecretpass"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.accessPointCalls.size());
    TEST_ASSERT_TRUE(fixture.view.contains("Failed to start Access Point"));
}

void test_probe_can_be_cancelled_before_service_start() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("probe"));

    TEST_ASSERT_TRUE(fixture.wifiOpenScanner.startIntervals.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Probe cancelled"));
}

void test_probe_starts_flushes_logs_and_stops_on_enter() {
    WifiFixture fixture;
    fixture.terminalInput.queueLine("y");
    fixture.wifiOpenScanner.logs.push_back({"open net found"});

    fixture.controller.handleCommand(TerminalCommand("probe"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifiOpenScanner.clearCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifiOpenScanner.startIntervals.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifiOpenScanner.stopCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("open net found"));
    TEST_ASSERT_TRUE(fixture.view.contains("Open-Wifi probe ended"));
}

void test_sniff_starts_switches_first_channel_and_stops() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("sniff"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.startPassiveSniffingCalls);
    TEST_ASSERT_FALSE(fixture.wifi.switchedChannels.empty());
    TEST_ASSERT_EQUAL_UINT8(1, fixture.wifi.switchedChannels[0]);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.stopPassiveSniffingCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("WiFi Sniffing stopped"));
}

void test_flood_valid_channel_prepares_raw_tx_and_stops() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("flood", "6"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.preparedRawChannels.size());
    TEST_ASSERT_EQUAL_UINT8(6, fixture.wifi.preparedRawChannels[0]);
    TEST_ASSERT_TRUE(fixture.view.contains("WiFi Flood: Stopped by user"));
}

void test_flood_rejects_invalid_channel_without_raw_tx() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("flood", "15"));

    TEST_ASSERT_TRUE(fixture.wifi.preparedRawChannels.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Invalid channel"));
}

void test_repeater_requires_existing_wifi_connection() {
    WifiFixture fixture;
    fixture.wifi.connected = false;

    fixture.controller.handleCommand(TerminalCommand("repeater", "start", "LabRepeater verysecurepass"));

    TEST_ASSERT_TRUE(fixture.wifi.repeaterCalls.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("WiFi not connected"));
}

void test_repeater_starts_from_saved_station_credentials() {
    WifiFixture fixture;
    fixture.wifi.connected = true;
    fixture.nvs.values[GlobalState::getInstance().getNvsSsidField()] = "Uplink";
    fixture.nvs.values[GlobalState::getInstance().getNvsPasswordField()] = "uplinkpass";

    fixture.controller.handleCommand(TerminalCommand("repeater", "start", "LabRepeater verysecurepass"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.repeaterCalls.size());
    const auto& call = fixture.wifi.repeaterCalls[0];
    TEST_ASSERT_EQUAL_STRING("Uplink", call.staSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("uplinkpass", call.staPass.c_str());
    TEST_ASSERT_EQUAL_STRING("LabRepeater", call.apSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("verysecurepass", call.apPass.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("Routing traffic"));
}

void test_deauth_preserves_spaced_ssid_target() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("deauth", "Lab", "WiFi"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.deauthSsids.size());
    TEST_ASSERT_EQUAL_STRING("Lab WiFi", fixture.wifi.deauthSsids[0].c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("Deauth frames sent"));
}

void test_webui_reports_serial_warning_and_network_ip_when_connected() {
    WifiFixture fixture;
    fixture.wifi.connected = true;
    fixture.wifi.localIp = "192.168.1.80";

    fixture.controller.handleCommand(TerminalCommand("webui"));

    TEST_ASSERT_TRUE(fixture.view.contains("WiFi Web UI: http://192.168.1.80"));
    TEST_ASSERT_TRUE(fixture.view.contains("Reset the device"));
}

void test_waterfall_draws_one_sample_then_stops() {
    WifiFixture fixture;
    fixture.terminalInput.queueReadChar(KEY_NONE);
    fixture.terminalInput.queueReadChar(KEY_OK);
    fixture.wifi.rssiSamples.push_back(-45);
    fixture.wifi.packetCounts.push_back(3);

    fixture.controller.handleCommand(TerminalCommand("waterfall"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.startPassiveSniffingCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.stopPassiveSniffingCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.device.waterfallCalls.size());
    TEST_ASSERT_EQUAL_INT(0, fixture.device.waterfallCalls[0].index);
    TEST_ASSERT_TRUE(fixture.device.waterfallCalls[0].level > 0);
}

void test_reset_delegates_to_service() {
    WifiFixture fixture;

    fixture.controller.handleCommand(TerminalCommand("reset"));

    TEST_ASSERT_EQUAL_UINT32(1, fixture.wifi.resetCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("Interface reset"));
}

void test_hostname_save_preserves_live_web_connection_and_credentials() {
    WifiFixture fixture;
    GlobalState::getInstance().setTerminalMode(TerminalTypeEnum::WiFiClient);
    fixture.wifi.connected = true;
    fixture.nvs.values["ssid"] = "Lab";
    fixture.nvs.values["pass"] = "secret";
    fixture.controller.handleCommand(TerminalCommand("config", "hostname", "My-Pirate"));
    TEST_ASSERT_EQUAL_STRING("My-Pirate", fixture.nvs.values["hostname"].c_str());
    TEST_ASSERT_EQUAL_STRING("Lab", fixture.nvs.values["ssid"].c_str());
    TEST_ASSERT_EQUAL_STRING("secret", fixture.nvs.values["pass"].c_str());
    TEST_ASSERT_EQUAL_STRING("buspirate", fixture.wifi.hostname.c_str());
    TEST_ASSERT_TRUE(fixture.wifi.connected);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.wifi.disconnectCalls);
    TEST_ASSERT_TRUE(fixture.wifi.connectCalls.empty());
    TEST_ASSERT_TRUE(fixture.view.contains("Restart your device"));
    GlobalState::getInstance().setTerminalMode(TerminalTypeEnum::SerialPort);
}

void test_hostname_invalid_input_does_not_overwrite_saved_value() {
    WifiFixture fixture;
    fixture.nvs.values["hostname"] = "old-name";
    fixture.controller.handleCommand(TerminalCommand("config", "hostname", "two names"));
    TEST_ASSERT_TRUE(fixture.nvs.savedStrings.empty());
    TEST_ASSERT_EQUAL_STRING("old-name", fixture.nvs.values["hostname"].c_str());
}

void test_hostname_status_distinguishes_saved_and_active_values() {
    WifiFixture fixture;
    fixture.nvs.values["hostname"] = "next-name";
    fixture.controller.handleCommand(TerminalCommand("config", "hostname"));
    TEST_ASSERT_TRUE(fixture.view.contains("Saved hostname: next-name"));
    TEST_ASSERT_TRUE(fixture.view.contains("Active hostname: buspirate"));
    TEST_ASSERT_TRUE(fixture.nvs.savedStrings.empty());
}

void test_hostname_storage_failure_is_reported() {
    WifiFixture fixture;
    fixture.nvs.failWrites = true;
    fixture.controller.handleCommand(TerminalCommand("config", "hostname", "new-name"));
    TEST_ASSERT_TRUE(fixture.view.contains("Failed to save hostname"));
    TEST_ASSERT_FALSE(fixture.view.contains("Hostname saved"));
}

}  // namespace wifi_controller_tests

void runWifiControllerTests() {
    using namespace wifi_controller_tests;
    RUN_TEST(test_hostname_save_preserves_live_web_connection_and_credentials);
    RUN_TEST(test_hostname_invalid_input_does_not_overwrite_saved_value);
    RUN_TEST(test_hostname_status_distinguishes_saved_and_active_values);
    RUN_TEST(test_hostname_storage_failure_is_reported);
    RUN_TEST(test_build_wifi_lines_reports_disconnected_mode);
    RUN_TEST(test_build_wifi_lines_reports_connected_ip_and_truncated_ssid);
    RUN_TEST(test_connect_with_inline_credentials_saves_nvs_on_success);
    RUN_TEST(test_connect_failure_resets_station_when_not_hotspot_terminal);
    RUN_TEST(test_connect_without_args_uses_confirmed_saved_credentials);
    RUN_TEST(test_disconnect_delegates_to_service);
    RUN_TEST(test_status_prints_station_details_and_connected_metrics);
    RUN_TEST(test_scan_formats_detailed_network_flags);
    RUN_TEST(test_scan_reports_empty_result);
    RUN_TEST(test_spoof_defaults_to_station_interface);
    RUN_TEST(test_spoof_rejects_invalid_interface_without_service_call);
    RUN_TEST(test_ap_start_uses_ap_only_mode_when_station_is_disconnected);
    RUN_TEST(test_ap_stop_delegates_to_service);
    RUN_TEST(test_ap_start_reports_service_failure);
    RUN_TEST(test_probe_can_be_cancelled_before_service_start);
    RUN_TEST(test_probe_starts_flushes_logs_and_stops_on_enter);
    RUN_TEST(test_sniff_starts_switches_first_channel_and_stops);
    RUN_TEST(test_flood_valid_channel_prepares_raw_tx_and_stops);
    RUN_TEST(test_flood_rejects_invalid_channel_without_raw_tx);
    RUN_TEST(test_repeater_requires_existing_wifi_connection);
    RUN_TEST(test_repeater_starts_from_saved_station_credentials);
    RUN_TEST(test_deauth_preserves_spaced_ssid_target);
    RUN_TEST(test_webui_reports_serial_warning_and_network_ip_when_connected);
    RUN_TEST(test_waterfall_draws_one_sample_then_stops);
    RUN_TEST(test_reset_delegates_to_service);
}
