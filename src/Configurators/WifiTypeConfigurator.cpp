#include "Configurators/WifiTypeConfigurator.h"
#include <States/GlobalState.h>
#include <Services/WifiService.h>

std::string WifiTypeConfigurator::configure(TerminalTypeEnum& terminalType) {
    switch (terminalType)
    {
    case TerminalTypeEnum::WiFiClient:
        #if defined(DEVICE_CARDPUTER)
            // Use this standalone setup for now
            setupCardputerWifi(); // endless loop until a valid WiFi is selected and connected
        #elif defined(DEVICE_STICKS3)
            // Use this standalone setup for now
            setupStickWifi(); // check stored creds
        #elif defined(DEVICE_TEMBEDS3) || defined(DEVICE_TEMBEDS3CC1101)
            // Use this standalone setup for now
            setupTembedWifi(view); // endless loop until a valid WiFi is selected and connected
        #elif defined(DEVICE_TDISPLAYS3)
            setupTdisplayWifi(view); // endless loop until a valid WiFi is selected and connected
        #elif defined(DEVICE_WAVESHARE_S3_GEEK)
            setupWaveshareS3GeekWifi(view); // endless loop until a valid WiFi is selected and connected
        #elif defined(DEVICE_VISION_MASTER_T190)
            setupVisionMasterT190Wifi(view);
        #else
            // Use this standalone setup for now
            setupDefaultWifi(); // check stored creds
        #endif

        return std::string(WiFi.localIP().toString().c_str());

    case TerminalTypeEnum::WiFiAp: {
        GlobalState& state = GlobalState::getInstance();
        std::string apSsid = state.getApName();
        state.setActiveApName(apSsid);

        WiFi.persistent(false);
        WiFi.disconnect(true);
        utilityService.sleepMs(100);
        if (!WifiService::setMode(WIFI_AP)) return "0.0.0.0";
        WiFi.setSleep(false);

        IPAddress apIp(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIp, gateway, subnet);

        constexpr int apChannel = 6;
        constexpr int maxConnections = 4;
        if (!WiFi.softAP(apSsid.c_str(), state.getApPassword(), apChannel, false, maxConnections)) {
            return "0.0.0.0";
        }

        return std::string(WiFi.softAPIP().toString().c_str());
    }
    default:
        break;
    }

    return "";
}
