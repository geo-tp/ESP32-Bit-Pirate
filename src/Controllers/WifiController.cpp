#include "Controllers/WifiController.h"
#include "Vendors/wifi_atks.h"

/*
Entry point for command
*/
void WifiController::handleCommand(const TerminalCommand &cmd)
{
    const auto &root = cmd.getRoot();

    if (root == "connect") handleConnect(cmd);
    else if (root == "disconnect") handleDisconnect(cmd);
    else if (root == "status") handleStatus(cmd);
    else if (root == "hostname") handleHostname(cmd);
    else if (root == "ap") handleAp(cmd);
    else if (root == "spam") handleApSpam();
    else if (root == "spoof") handleSpoof(cmd);
    else if (root == "scan") handleScan(cmd);
    else if (root == "waterfall") handleWaterfall();
    else if (root == "probe") handleProbe();
    else if (root == "ping") handlePing(cmd);
    else if (root == "sniff") handleSniff(cmd);
    else if (root == "webui") handleWebUi(cmd);
    else if (root == "ssh") handleSsh(cmd);
    else if (root == "telnet") handleTelnet(cmd);
    else if (root == "nc") handleNetcat(cmd);
    else if (root == "nmap") handleNmap(cmd);
    else if (root == "modbus") handleModbus(cmd);
    else if (root == "http") handleHttp(cmd);
    else if (root == "lookup") handleLookup(cmd);
    else if (root == "discovery") handleDiscovery(cmd);
    else if (root == "flood") handleFlood(cmd);
    else if (root == "repeater" || root == "extender") handleRepeater(cmd);  
    else if (root == "reset") handleReset();
    else if (root == "deauth") handleDeauth(cmd);
    else handleHelp();
}

std::vector<std::string> WifiController::buildWiFiLines() {
    std::vector<std::string> lines;
    lines.reserve(4);

    int mode   = wifiService.getWifiModeRaw();
    int status = wifiService.getWifiStatusRaw();

    // MODE
    lines.push_back(
        std::string("MODE ") + IWifiService::wifiModeToStr(mode)
    );

    // Disconnection
    if (status != IWifiService::kWifiStatusConnected) {
        lines.push_back("WIFI DISCONNECTED");
        return lines;
    }

    // Connected
    lines.push_back("WIFI CONNECTED");

    // STA IP
    std::string staIp = wifiService.getLocalIP();
    if (!staIp.empty())
        lines.push_back(staIp);

    // SSID
    std::string ssid = wifiService.getSsid();
    if (!ssid.empty()) {
        std::string nameLimited = ssid.length() > 15 ? ssid.substr(0, 15) + "..." : ssid;
        lines.push_back(nameLimited);
    }

    return lines;
}

/*
Connect
*/
void WifiController::handleConnect(const TerminalCommand &cmd)
{
    std::string ssid;
    std::string password;
    auto args = argTransformer.splitArgs(cmd.getSubcommand());

    // No args provided, we need to check saved creds or scan and select networks
    if (cmd.getSubcommand().empty()) {

        // Check saved creds
        nvsService.open();
        ssid = nvsService.getString(state.getNvsSsidField());
        password = nvsService.getString(state.getNvsPasswordField());
        nvsService.close();
        auto confirmation = false;

        // Creds found
        if (!ssid.empty() && !password.empty()) {
            confirmation = userInputManager.readYesNo(
                "WiFi: Use saved credentials for " + ssid + "? (Y/n)", true
            );
        } 

        // Select network if no creds or not confirmed
        if (!confirmation) {
            if (state.getTerminalMode() == TerminalTypeEnum::WiFiAp) {
                wifiService.recoverStaForRetry(true);
            }

            terminalView.println("Wifi: Scanning for available networks...");
            auto networks = wifiService.scanNetworks();
            if (networks.empty()) {
                terminalView.println("No Wi-Fi networks found.\n");
                return;
            }

            networks.push_back("Exit");
            int selectedIndex = userInputManager.readValidatedChoiceIndex("\nSelect Wi-Fi network", networks, 0);
            if (selectedIndex == networks.size() - 1) {
                terminalView.println("Exiting network selection....\n");
                return;
            }
            ssid = networks[selectedIndex];
            terminalView.println("Selected SSID: " + ssid);
            terminalView.print("Password: ");
            password = userInputManager.getLine();
        }

    // Args provided
    } else  {
        // Concatenate subcommand and args
        std::string full = cmd.getSubcommand() + " " + cmd.getArgs();
    
        // Find the last space to separate SSID and password
        size_t pos = full.find_last_of(' ');
        if (pos == std::string::npos || pos == full.size() - 1) {
            terminalView.println("Usage: connect <ssid> <password>");
            return;
        }
        ssid = full.substr(0, pos);
        password = full.substr(pos + 1);
    }

    terminalView.println("WiFi: Connecting to " + ssid + "...");

    wifiService.setModeApSta();
    wifiService.connect(ssid, password);
    if (wifiService.isConnected()) {
        terminalView.println("\nWiFi: ✅ Connected successfully.");
        handleWebUi(cmd);

        // Save creds
        nvsService.open();
        nvsService.saveString(state.getNvsSsidField(), ssid);
        nvsService.saveString(state.getNvsPasswordField(), password);
        nvsService.close();
    } else {
        terminalView.println("WiFi: Connection failed.");
        if (state.getTerminalMode() == TerminalTypeEnum::WiFiAp) {
            wifiService.recoverStaForRetry(true);
            terminalView.println("WiFi Hotspot: Still available at http://" + wifiService.getApIp());
        } else {
            wifiService.reset();
            utilityService.sleepMs(100);
        }
    }
}

/*
Disconnect
*/
void WifiController::handleDisconnect(const TerminalCommand &cmd)
{
    wifiService.disconnect();
    terminalView.println("WiFi: Disconnected.");
}

/*
Status
*/
void WifiController::handleStatus(const TerminalCommand &cmd)
{
    auto ssid     = wifiService.getSsid();     if (ssid.empty()) ssid = "N/A";
    auto bssid    = wifiService.getBssid();    if (bssid.empty()) bssid = "N/A";
    auto hostname = wifiService.getHostname(); if (hostname.empty()) hostname = "N/A";

    terminalView.println("\n=== Wi-Fi Status ===");
    terminalView.println("Mode         : " + std::string(wifiService.getWifiModeRaw() == IWifiService::kWifiModeAp ? "Access Point" : "Station"));
    terminalView.println("AP MAC       : " + wifiService.getMacAddressAp());
    terminalView.println("STA MAC      : " + wifiService.getMacAddressSta());
    terminalView.println("IP           : " + wifiService.getLocalIp());
    terminalView.println("Subnet       : " + wifiService.getSubnetMask());
    terminalView.println("Gateway      : " + wifiService.getGatewayIp());
    terminalView.println("DNS1         : " + wifiService.getDns1());
    terminalView.println("DNS2         : " + wifiService.getDns2());
    terminalView.println("Hostname     : " + hostname);

    terminalView.println("SSID         : " + ssid);
    terminalView.println("BSSID        : " + bssid);
    terminalView.println("Prov enabled : " + std::string(wifiService.isProvisioningEnabled() ? "Yes" : "No"));

    const int status = wifiService.getWifiStatusRaw();
    if (status == IWifiService::kWifiStatusConnected) {
        terminalView.println("RSSI         : " + std::to_string(wifiService.getRssi()) + " dBm");
        terminalView.println("Channel      : " + std::to_string(wifiService.getChannel()));
    } else {
        terminalView.println("RSSI         : N/A");
        terminalView.println("Channel      : N/A");
    }

    terminalView.println("Mode         : " + std::string(IWifiService::wifiModeToStr(wifiService.getWifiModeRaw())));
    terminalView.println("Status       : " + std::string(IWifiService::wlStatusToStr(status)));
    terminalView.println("====================\n");
}

/*
Hostname
*/
void WifiController::handleHostname(const TerminalCommand &cmd)
{
    const std::string hostname = cmd.getSubcommand();

    if (hostname.empty()) {
        terminalView.println("Usage: hostname <name>");
        return;
    }

    if (hostname.length() > 32) {
        terminalView.println("Invalid hostname. Maximum 32 characters.");
        return;
    }

    if (hostname.front() == '-' || hostname.back() == '-') {
        terminalView.println("Invalid hostname. Cannot start or end with '-'.");
        return;
    }

    for (char c : hostname) {
        if (!(argTransformer.isValidAlphanumeric(std::string(1, c)) || c == '-')) {
            terminalView.println("Invalid hostname. Use letters, numbers, and '-' only, ");
            return;
        }
    }



    // Save hostname to NVS and set it
    nvsService.open();
    nvsService.saveString(GlobalState::getInstance().getNvsHostnameField(), hostname);
    nvsService.close();
    WiFi.setHostname(hostname.c_str());

    terminalView.println("WiFi: Hostname set to: " + wifiService.getHostname());
    terminalView.println("\n [⚠️  WARNING] ");
    terminalView.println(" Restart your device for hostname change to take effect.*\n");
}

/*
Access Point
*/
void WifiController::handleAp(const TerminalCommand &cmd)
{
    auto ssid = cmd.getSubcommand();

    if (ssid.empty())
    {
        terminalView.println("Usage: ap <ssid> <password>");
        terminalView.println("       ap spam");
        return;
    }

    if (ssid == "spam") {
        handleApSpam();
        return;
    }
    
    if (ssid == "stop") {
        wifiService.stopAccessPoint();
        terminalView.println("WiFi: Access Point stopped.\n");
        return;
    }

    auto full = cmd.getSubcommand() + " " + cmd.getArgs();

    // Find the last space to separate SSID and password
    size_t pos = full.find_last_of(' ');
    if (pos == std::string::npos || pos == full.size() - 1) {
        terminalView.println("Usage: ap <ssid> <password>");
        return;
    }
    ssid = full.substr(0, pos);
    auto password = full.substr(pos + 1);

    // Confirm forwarding
    if (wifiService.isConnected()) {
        auto forward = userInputManager.readYesNo("Enable internet access forwarding ?", true);
        if (forward) {
            handleRepeater(cmd);
            return;
        }
    }

    // Already connected, mode AP+STA
    if (wifiService.isConnected())
    {
        wifiService.setModeApSta();
    }
    else
    {
        wifiService.setModeApOnly();
    }

    if (wifiService.startAccessPoint(ssid, password))
    {
        terminalView.println("\nWiFi: Access Point is started, no forwarding...\n");
        terminalView.println("  SSID            : " + ssid);
        std::string apPassMasked = password.empty() ? "" : std::string(password.length(), '*');
        std::string first2 = password.substr(0, password.size() >= 2 ? 2 : 1);
        apPassMasked = first2 + "********" + std::string(1, password.back()); 
        terminalView.println("  Password        : " + (apPassMasked.empty() ? "(open)" : apPassMasked));
        terminalView.println("  Access point IP : " + wifiService.getApIp());

        auto nvsSsidField = state.getNvsSsidField();
        auto nvsPasswordField = state.getNvsPasswordField();
        auto ssid = nvsService.getString(nvsSsidField, "");
        auto password = nvsService.getString(nvsPasswordField, "");

        // Try to reconnect to saved WiFi
        if (!ssid.empty() && !password.empty())
        {
            wifiService.connect(ssid, password);
        }

        if (wifiService.isConnected())
        {
            terminalView.println("  Station IP      : " + wifiService.getLocalIp());
        }
        terminalView.println("");

        terminalView.println("  Use 'ap stop' to stop the access point\n");
    }
    else
    {
        terminalView.println("WiFi: Failed to start Access Point.");
    }
}

/*
AP Spam
*/
void WifiController::handleApSpam()
{
    terminalView.println("WiFi: Starting beacon spam... Press [ENTER] to stop.");

    if (!wifiService.prepareRawTx(1)) {
        terminalView.println("WiFi: Failed to prepare raw packet injection.\n");
        return;
    }

    while (true)
    {
        beaconCreate("", 0, 0); // func from Vendors/wifi_atks.h

        // Enter press to stop
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;
        utilityService.sleepMs(10);
    }

    terminalView.println("WiFi: Beacon spam stopped.\n");
}

/*
Scan
*/
void WifiController::handleScan(const TerminalCommand &)
{
    terminalView.println("WiFi: Scanning for networks...");
    utilityService.sleepMs(300);

    auto networks = wifiService.scanDetailedNetworks();

    for (const auto &net : networks)
    {
        std::string line = "  SSID: " + net.ssid;
        line += " | Sec: " + wifiService.encryptionTypeToString(net.encryption);
        line += " | BSSID: " + net.bssid;
        line += " | CH: " + std::to_string(net.channel);
        line += " | RSSI: " + std::to_string(net.rssi) + " dBm";
        if (net.open)
            line += " [open]";
        if (net.vulnerable)
            line += " [vulnerable]";
        if (net.hidden)
            line += " [hidden]";

        terminalView.println(line);
    }

    if (networks.empty())
    {
        terminalView.println("WiFi: No networks found.");
    }
}

/*
Probe
*/
void WifiController::handleProbe() 
{
    terminalView.println("WIFI: Starting probe for internet access on open networks...");
    terminalView.println("\n [⚠️  WARNING] ");
    terminalView.println(" This will try to connect to surrounding open networks.\n");

    // Confirm before starting
    auto confirmation = userInputManager.readYesNo("Start Wi-Fi probe to find internet access?", false);
    if (!confirmation) {
        terminalView.println("WIFI: Probe cancelled.\n");
        return;
    }

    // Stop any existing probe
    if (wifiOpenScannerService.isOpenProbeRunning()) {
        wifiOpenScannerService.stopOpenProbe();
    }
    wifiOpenScannerService.clearProbeLog();

    // Start the open probe service
    if (!wifiOpenScannerService.startOpenProbe()) {
        terminalView.println("WIFI: Failed to start probe.\n");
        return;
    }

    terminalView.println("WIFI: Probe for internet access... Press [ENTER] to stop.\n");

    // Start the open probe task
    while (wifiOpenScannerService.isOpenProbeRunning()) {
        // Display logs
        auto batch = wifiOpenScannerService.fetchProbeLog();
        for (auto& ln : batch) {
            terminalView.println(ln.c_str());
        }

        // Enter Press to stop
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            wifiOpenScannerService.stopOpenProbe();
            break;
        }

        utilityService.sleepMs(10);
    }

    // Flush final logs
    for (auto& ln : wifiOpenScannerService.fetchProbeLog()) {
        terminalView.println(ln.c_str());
    }
    terminalView.println("WIFI: Open-Wifi probe ended.\n");
}

/*
Sniff
*/
void WifiController::handleSniff(const TerminalCommand &cmd)
{
    terminalView.println("WiFi Sniffing started... Press [ENTER] to stop.\n");

    wifiService.startPassiveSniffing();
    wifiService.switchChannel(1);

    uint8_t channel = 1;
    unsigned long lastHop = 0;
    unsigned long lastPull = 0;

    while (true)
    {
        // Enter Press
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n')
            break;

        // Read sniff data
        if (utilityService.nowMs() - lastPull > 20)
        {
            auto logs = wifiService.getSniffLog();
            for (const auto &line : logs)
            {
                terminalView.println(line);
            }
            lastPull = utilityService.nowMs();
        }

        // Switch channel every 100ms
        if (utilityService.nowMs() - lastHop > 100)
        {
            channel = (channel % 13) + 1; // channel 1 to 13
            wifiService.switchChannel(channel);
            lastHop = utilityService.nowMs();
        }

        utilityService.sleepMs(5);
    }

    wifiService.stopPassiveSniffing();
    terminalView.println("WiFi Sniffing stopped.\n");
}

/*
Spoof
*/
void WifiController::handleSpoof(const TerminalCommand &cmd)
{
    auto mode = cmd.getSubcommand();
    auto mac  = cmd.getArgs();

    if (mode.empty() && mac.empty())
    {
        terminalView.println("Usage: spoof sta 02:AA:BB:CC:DD:EE");
        terminalView.println("       spoof ap 02:AA:BB:CC:DD:EE");
        return;
    }

    // user entered: spoof <mac> -> assume station mode
    if (mac.empty())
    {
        mac = mode;
        mode = "sta";
    }

    if (mode != "sta" && mode != "ap")
    {
        terminalView.println("Invalid mode. Use 'sta' or 'ap'.");
        return;
    }

    WifiMacInterface iface =
        (mode == "sta")
            ? WifiMacInterface::Station
            : WifiMacInterface::AccessPoint;

    terminalView.println("WiFi: Spoofing " + mode + " MAC to " + mac + "...");

    bool ok = wifiService.spoofMacAddress(mac, iface);

    if (ok)
    {
        terminalView.println("WiFi: MAC spoofed successfully.");
    }
    else
    {
        terminalView.println("WiFi: Failed to spoof. Use a valid unicast MAC (ex: 02:AA:BB:CC:DD:EE).");
    }
}

/*
Repeater
*/
void WifiController::handleRepeater(const TerminalCommand& cmd)
{
    // Usage:
    // repeater
    // repeater [ap_ssid] [ap_pass]
    // repeater stop
    // repeater start (prompt for ap_ssid/ap_pass)
    // repeater start [ap_ssid] [ap_pass]

    std::string sub = cmd.getSubcommand();
    std::string args = cmd.getArgs();
    std::string apSsid = "";
    std::string apPass = "";
    std::string apPassMasked = "";
    const uint8_t maxConn = 10;

    // Must be connected first
    if (!wifiService.isConnected()) {
        terminalView.println("WiFi Repeater: WiFi not connected. Run 'connect' first.\n");
        return;
    }

    // Status   
    if (sub.empty() ) {
        sub = wifiService.isRepeaterRunning() ? "stop" : "start";
    }

    if (sub == "stop") {
        wifiService.stopRepeater();
        terminalView.println("WiFi Repeater: Stop routing traffic between uplink and repeater.\n");
        return;
    }

    if (sub != "start") {
        if (!args.empty())
            args = sub + " " + args;
        else
            args = sub;
    }

    // Parse ap ssid/pass from args
    if (!args.empty()) {
        auto parts = argTransformer.splitArgs(args);

        if (parts.size() >= 1) apSsid = parts[0];
        if (parts.size() >= 2) apPass = parts[1];

        // If SSID had spaces, keep last token as pass and rest as SSID
        if (parts.size() > 2) {
            apPass = parts.back();
            apSsid.clear();
            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                if (i) apSsid += " ";
                apSsid += parts[i];
            }
        }
    }

    // Prompt for missing info
    if (apSsid.empty()) {
        terminalView.println("\nWiFi Repeater: Forwarding traffic from uplink.");
        apSsid = userInputManager.readSanitizedString(
            "Enter Repeater SSID", 
            "esp32repeater", 
            /*onlyLetter=*/false
        );
        apSsid = apSsid.size() > 32 ? apSsid.substr(0, 32) : apSsid;
    }

    if (apPass.empty()) {
        apPass = userInputManager.readSanitizedString(
            "Enter Repeater Pass", 
            "esp32bitpirate",
            /*onlyLetter=*/false
        );
        if (apPass.size() > 64) {
            terminalView.println("Password must be at most 64 chars. Length reduced.");
            apPass = apPass.substr(0, 64);
        }
    }

    if (apPass.length() < 12 && !apPass.empty()) {
        terminalView.println("Password must be at least 12 characters.");
        return;
    }

    // at this point, password can't be empty
    std::string first2 = apPass.substr(0, apPass.size() >= 2 ? 2 : 1);
    apPassMasked = first2 + "********" + std::string(1, apPass.back());

    // Read current uplink creds from NVS
    std::string staSsid;
    std::string staPass;
    nvsService.open();
    staSsid = nvsService.getString(state.getNvsSsidField());
    staPass = nvsService.getString(state.getNvsPasswordField());
    nvsService.close();

    if (staSsid.empty()) {
        // fallback to current SSID if NVS empty
        staSsid = wifiService.getSsid();
    }

    if (staSsid.empty()) {
        terminalView.println("WiFi Repeater: WiFi not connected, run 'connect' first.\n");
        return;
    }

    terminalView.println("\nWiFi Repeater: Starting repeater...\n");

    // Start NAT repeater
    bool ok = wifiService.startRepeater(
        staSsid,
        staPass,
        apSsid,
        apPass,
        /*apChannel=*/1,
        /*maxConn=*/maxConn,
        /*timeoutMs=*/15000
    );

    if (!ok) {
        terminalView.println("\nWiFi Repeater: Failed to start. Abort\n");
        return;
    }

    terminalView.println("WiFi Repeater: Routing traffic between uplink and repeater...");
    terminalView.println("\n  Uplink           : " + staSsid);
    terminalView.println("  Repeater SSID    : " + apSsid);
    terminalView.println("  Repeater Pass    : " + std::string(apPassMasked.empty() ? "(open)" :  apPassMasked));
    terminalView.println("  Repeater IP      : " + wifiService.getRepeaterIp());
    terminalView.println("  Max connections  : " + std::to_string(maxConn));
    terminalView.println("\n  Use 'repeater stop' to stop.");
    terminalView.println("");
}

/*
Flood
*/
void WifiController::handleFlood(const TerminalCommand& cmd)
{   
    // Channel
    uint8_t channel = 0;
    if (cmd.getSubcommand().empty()) {
        // prompt for channel
        channel = userInputManager.readValidatedUint8("Enter channel to flood (1-14)", 1, 1, 14);
    } else {
        // parse channel from subcommand
        if (argTransformer.isValidNumber(cmd.getSubcommand())) {
            channel = argTransformer.toUint8(cmd.getSubcommand());
            if (channel < 1 || channel > 14) {
                terminalView.println("Invalid channel. Must be between 1 and 14.");
                return;
            }
        } else {
            terminalView.println("Usage: flood [channel]");
            return;
        }
    }
     
    terminalView.println("\nWiFi Flood: Starting on channel " + std::to_string(channel) + "... Press [ENTER] to stop.");

    if (!wifiService.prepareRawTx(channel)) {
        terminalView.println("WiFi Flood: Failed to prepare raw packet injection.\n");
        return;
    }

    while (true) {
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') break;
        beaconCreate("", channel, 0); // func from Vendors/wifi_atks.h
     }

    terminalView.println("WiFi Flood: Stopped by user.\n");
}

/*
Waterfall
*/
void WifiController::handleWaterfall()
{
    std::string title = "Peak: --";
    uint16_t pktDwellMs = userInputManager.readValidatedInt("Hold time per channel (ms)", 50, 5, 500);
    
    // Scale packet count to keep waterfall bars visually 
    // consistent across different dwell times
    // reference is [dwell 80ms, 1 packet = +5]
    // meaning 10 packets received in 80ms = max score
    const float refDwellMs = 80.0f;
    const float refMul = 5.0f;
    const float timeScale = refDwellMs / (float)pktDwellMs; 

    terminalView.println("\nWiFi Waterfall: Displaying on the ESP32 screen... Press [ENTER] to stop.");
    wifiService.startPassiveSniffing();

    while (true)
    {
        int8_t bestCh = -1;
        int8_t bestRssi = -127;
        int8_t bestLevel = -1;
        int8_t channel = -1;

        for (uint8_t idx = 0; idx < 13; ++idx)
        {
            channel = idx + 1;

            // Enter press to stop
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') {
                terminalView.println("WiFi Waterfall: Stopped by user.\n");
                wifiService.stopPassiveSniffing();
                return;
            }

            // Get RSSI on channel
            int8_t rssi = wifiService.scanRssiOnChannel(channel);

            // Get packet count on channel
            uint32_t pkts = wifiService.countPacketsOnChannel(channel, pktDwellMs);

            // RSSI, score 0 to 50
            int rssiPart = 0;
            if (rssi != -127) {
                int rr = rssi;
                if (rr < -100) rr = -100;
                if (rr > -30)  rr = -30;
                rssiPart = (rr + 100) * 50 / 70;

                if (rssiPart < 0) rssiPart = 0;
                if (rssiPart > 50) rssiPart = 50;
            }

            // Packets, score 0 to 50
            int pktPart = (int)((float)pkts * refMul * timeScale);
            if (pktPart > 50) pktPart = 50;
            if (pktPart < 0)  pktPart = 0;

            // Final level
            // 50% RSSI and 50% packets
            int8_t level = rssiPart + pktPart;
            if (level < 1) level = 1; // show progress

            // Draw the channel line
            deviceView.drawWaterfall(
                title,
                1.0f, 13.0f,
                "ch",
                idx,
                13,
                level
            );

            // printed each start of loop, hack to reset after printing
            if (idx == 0) title = "Peak: --";

            // best peak
            if (rssi != -127 && (level > bestLevel)) {
                bestLevel = level;
                bestRssi = rssi;
                bestCh = channel;

                title = "Peak: CH" + std::to_string(bestCh) +
                        " (" + std::to_string(2407 + bestCh * 5) + "MHz) " +
                        std::to_string(bestRssi) + " dBm";
            }

        }
    }
}

/*
Reset
*/
void WifiController::handleReset()
{
    wifiService.reset();
    terminalView.println("WiFi: Interface reset. Disconnected.");
}

/*
Web Interface
*/
void WifiController::handleWebUi(const TerminalCommand &)
{
    if (state.getTerminalMode() == TerminalTypeEnum::WiFiAp)
    {
        terminalView.println("");
        if (wifiService.isConnected()) {
            terminalView.println("WiFi: Connected to " + wifiService.getSsid() + ".");
            terminalView.println("WiFi Hotspot: Still running for this setup session.");
            terminalView.println("");
            terminalView.println("Web CLI addresses:");
            terminalView.println("  Hotspot : http://" + wifiService.getApIp());
            terminalView.println("  Network : http://" + wifiService.getLocalIP());
            terminalView.println("  Tip: You have to reset and select WiFi Connect");
        } else {
            terminalView.println("WiFi Hotspot UI:");
            terminalView.println("  Hotspot : http://" + wifiService.getApIp());
        }
        terminalView.println("");
        terminalView.println("Hotspot credentials:");
        terminalView.println("  SSID    : " + state.getActiveApName());
        terminalView.println("  Password: " + std::string(state.getApPassword()));
        terminalView.println("  Captive : open any URL from a device connected to the hotspot");
        terminalView.println("");
        return;
    }

    if (wifiService.isConnected())
    {
        auto ip = wifiService.getLocalIP();
        terminalView.println("");
        terminalView.println(" [⚠️  WARNING] ");
        terminalView.println(" If you're connected via serial,");
        terminalView.println(" the web UI will not be active.");
        terminalView.println(" Reset the device and choose WiFi Connect.");
        terminalView.println("");
        terminalView.println("[BAREBONE] To launch the WebUI without a screen:");
        terminalView.println("  1. Reset the device (don’t hold the board button during boot)");
        terminalView.println("  2. Once powered, press the board button within 3 seconds:");
        terminalView.println("     • Short press = WiFi Connect");
        terminalView.println("     • Hold press  = WiFi Hotspot");
        terminalView.println("  3. The built-in LED shows the following status:");
        terminalView.println("     • Blue  = No Wi-Fi credentials saved.");
        terminalView.println("     • White = Connecting in progress");
        terminalView.println("     • Green = Connected, open the WebUI in your browser.");
        terminalView.println("     • Red   = Connection failed, try connect again with serial");
        terminalView.println("");
        terminalView.println("WiFi Web UI: http://" + ip + " (reset and select WiFi Connect)");
    }
    else
    {
        terminalView.println("WiFi Web UI: Not connected. Connect first to see address.");
    }
}

/*
Config
*/
void WifiController::handleConfig()
{
    if (state.getTerminalMode() == TerminalTypeEnum::WiFiClient) {
        terminalView.println(" [⚠️  WARNING] ");
        terminalView.println(" You are connected via the Web CLI,");
        terminalView.println(" executing some Wi-Fi commands will cause ");
        terminalView.println(" the terminal session to disconnect.");
        terminalView.println(" Don't use: sniff, probe, connect, scan, spoof...");
        terminalView.println(" Use USB serial or restart if connection is lost.\n");
    } else if (state.getTerminalMode() == TerminalTypeEnum::WiFiAp) {
        terminalView.println(" [⚠️  WARNING] ");
        terminalView.println(" You are in Wi-Fi Hotspot mode, some commands");
        terminalView.println(" may cause the hotspot to stop or disconnect.");
        terminalView.println(" Don't use: sniff, ap, flood, spoof, reset...");
        terminalView.println(" Use USB serial or restart if connection is lost.\n");
    }
}

/*
Help
*/
void WifiController::handleHelp()
{
    terminalView.println("\nUnknown command. Available WiFi commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Ensure Configuration
*/
void WifiController::ensureConfigured()
{
    if (!configured)
    {
        handleConfig();
        configured = true;
    }
}

/*
Deauthenticate stations attack
*/
void WifiController::handleDeauth(const TerminalCommand &cmd)
{   
    auto target = cmd.getSubcommand();
    
    // Select network if no target provided
    if (target.empty()) {
        terminalView.println("Wifi: Scanning for available networks...");
        auto networks = wifiService.scanNetworks();
        int selectedIndex = userInputManager.readValidatedChoiceIndex("\nSelect Wi-Fi network", networks, 0);
        target = networks[selectedIndex];
    }

    // if the SSID have space in name, e.g "Router Wifi"
    if (!cmd.getArgs().empty())
    {
        target += " " + cmd.getArgs();
    }

    terminalView.println("WiFi: Sending deauth to \"" + target + "\"...");

    bool ok = wifiService.deauthApBySsid(target);

    if (ok)
        terminalView.println("WiFi: Deauth frames sent.");
    else
        terminalView.println("WiFi: SSID not found.");
}
