#pragma once

#include <WiFi.h>
#include <string>
#include <vector>
#include <sstream>
#include "Interfaces/IWifiService.h"

extern "C" {
  #include "esp_wifi.h"
  #include "esp_wifi_types.h"
}

struct SniffedPacket {
    int8_t rssi;
    uint8_t channel;
    std::string mac;
    uint8_t type;
};

class WifiService : public IWifiService {
public:

    using MacInterface = WifiMacInterface;
    
    WifiService();

    // Call once at boot, before any Wi-Fi mode or connection is started.
    static bool setStartupHostname(const std::string& hostname);

    // Connection
    bool connect(const std::string& ssid, const std::string& password, unsigned long timeoutMs = 15000) override;
    void disconnect() override;
    bool isConnected() const override;
    bool connected = false;
    bool repeater = false;

    // Utils
    std::string getLocalIP() const override;
    std::string getCurrentIP() const override;
    std::string getSubnetMask() const override;
    std::string getGatewayIp() const override;
    std::string getDns1() const override;
    std::string getDns2() const override;
    std::string getHostname() const override;
    void setModeApSta() override;
    void setModeApOnly() override;
    std::string getMacAddressSta() const override;
    std::string getMacAddressAp() const override;
    std::string getApIp() const override;
    std::string getLocalIp() const override;
    int getRssi() const override;
    int getChannel() const override;
    std::string getSsid() const override;
    std::string getBssid() const override;
    int getWifiModeRaw() const override;   // wifi_mode_t
    int getWifiStatusRaw() const override; // wl_status_t
    bool isProvisioningEnabled() const override;
    void reset() override;
    void recoverStaForRetry(bool keepApMode) override;
    bool prepareRawTx(uint8_t channel = 1) override;
    
    // Access point
    bool startAccessPoint(const std::string& ssid, const std::string& password = "", int channel = 1, int maxConn = 4) override;
    bool stopAccessPoint() override;

    // Spoof MAC
    bool spoofMacAddress(const std::string& macStr, MacInterface which) override;
    static std::string formatMac(const uint8_t* mac);

    // Scan
    std::vector<std::string> scanNetworks() override;
    std::vector<WiFiNetwork> scanDetailedNetworks() override;
    std::vector<WiFiNetwork> getOpenNetworks(const std::vector<WiFiNetwork>& networks);
    std::vector<WiFiNetwork> getVulnerableNetworks(const std::vector<WiFiNetwork>& networks);
    bool isVulnerable(int encryption) const;
    std::string encryptionTypeToString(int encryption) override;
    int8_t scanRssiOnChannel(uint8_t channel) override;
    uint32_t countPacketsOnChannel(uint8_t channel, uint16_t dwellMs) override;
    static void pktCountCb(void* buf, wifi_promiscuous_pkt_type_t);
    inline static volatile uint32_t g_pktCount = 0; // rx callback

    // Sniffing passif
    void startPassiveSniffing() override;
    void stopPassiveSniffing() override;
    std::vector<std::string> getSniffLog() override;
    bool switchChannel(uint8_t channel) override;
    static std::string getFrameTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype);
    static std::string parseSsidFromPacket(const uint8_t* payload, int len, uint8_t type, uint8_t subtype);
    static std::string getFrameTypeName(uint8_t type, uint8_t subtype);
    static void extractTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype);

    // Deathentication attacks
    bool deauthApBySsid (const std::string& ssid) override;
    void deauthAttack(const uint8_t bssid[6], uint8_t channel, uint8_t bursts, uint32_t sniffMs);

    // Client sniffer
    static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static void clientSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    inline static std::vector<std::string> sniffLog = {};
    inline static portMUX_TYPE sniffMux = portMUX_INITIALIZER_UNLOCKED;
    inline static portMUX_TYPE staMux = portMUX_INITIALIZER_UNLOCKED;
    inline static std::vector<std::array<uint8_t, 6>> staList = {};
    inline static uint8_t apBSSID[6] = {};
    
    // Repeater
    bool startRepeater(const std::string& staSsid,
                    const std::string& staPass,
                    const std::string& apSsid,
                    const std::string& apPass,
                    int apChannel = 1,
                    int maxConn = 10,
                    unsigned long timeoutMs = 15000) override;

    void stopRepeater() override;
    bool isRepeaterRunning() const override;
    std::string getRepeaterIp() const override;

    // Wifi mode to string
    static inline const char* wifiModeToStr(int m) {
        switch (static_cast<wifi_mode_t>(m)) {
            case WIFI_MODE_NULL:   return "NULL";
            case WIFI_MODE_STA:    return "STA";
            case WIFI_MODE_AP:     return "AP";
            case WIFI_MODE_APSTA:  return "AP+STA";
            default:               return "?";
        }
    }

    // Wifi status to string
    static inline const char* wlStatusToStr(int s) {
        switch (s) {
            case WL_IDLE_STATUS:         return "IDLE";
            case WL_NO_SSID_AVAIL:       return "NO_SSID";
            case WL_SCAN_COMPLETED:      return "SCAN_DONE";
            case WL_CONNECTED:           return "CONNECTED";
            case WL_CONNECT_FAILED:      return "CONNECT_FAILED";
            case WL_CONNECTION_LOST:     return "CONNECTION_LOST";
            case WL_DISCONNECTED:        return "DISCONNECTED";
            case WL_NO_SHIELD:           return "NO_SHIELD";
            default:                     return "?";
        }
    }
};
