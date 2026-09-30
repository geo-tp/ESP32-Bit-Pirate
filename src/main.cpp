#ifndef UNIT_TEST

#include <Views/SerialTerminalView.h>
#include <Views/WebTerminalView.h>
#include <Boards/Common/Views/NoScreenDeviceView.h>
#include <Inputs/SerialTerminalInput.h>
#include <Inputs/WebTerminalInput.h>
#include <Boards/Cardputer/CardputerBoard.h>
#include <Boards/Cardputer/CardputerDeviceView.h>
#include <Boards/Cardputer/CardputerInput.h>
#include <Boards/Cardputer/CardputerTerminalView.h>
#include <Boards/StickS3/StickS3Board.h>
#include <Boards/StampS3/StampS3Board.h>
#include <Boards/S3DevKit/S3DevKitBoard.h>
#include <Boards/UMProS3/UMProS3Board.h>
#include <Boards/Common/Inputs/DefaultInput.h>
#include <Boards/TDisplayS3/TDisplayS3Board.h>
#include <Boards/WaveshareS3Geek/WaveshareS3GeekBoard.h>
#include <Boards/TEmbed/TEmbedBoard.h>
#include <Boards/VisionMasterT190/VisionMasterT190Board.h>
#include <Boards/Custom/CustomBoard.h>
#include <Boards/Common/Serial/BoardHostSerial.h>
#include <Providers/DependencyProvider.h>
#include <Dispatchers/ActionDispatcher.h>
#include <Servers/HttpServer.h>
#include <Servers/WebSocketServer.h>
#include <Servers/DnsServer.h>
#include <Services/NvsService.h>
#include <Services/UtilityService.h>
#include <Selectors/HorizontalSelector.h>
#include <Configurators/TerminalTypeConfigurator.h>
#include <Configurators/WifiTypeConfigurator.h>
#include <Configurators/BootModeConfigurator.h>
#include <Enums/TerminalTypeEnum.h>
#include <States/GlobalState.h>

/*
This file initializes the selected board, lets the board expose its device view,
physical input and host serial link, selects the terminal mode, and then launches
the main loop through the ActionDispatcher.

- Terminal View: the generic CLI interface where the user SEES output.
    * SerialTerminalView    -> text terminal via the board host serial link.
    * WebTerminalView       -> text terminal in a browser (via WebSocket).
    * CardputerTerminalView -> Cardputer LCD acts as the terminal screen.

- Device View: the interface for device’s screen (if any).
    * M5DeviceView, St7789SpiDeviceView, St7789ParallelDeviceView, CardputerDeviceView, NoScreenDeviceView, etc.
    * Used for UI elements like mode, pinout mapping, or logic traces.

- Terminal Input: how the user TYPES commands into the system.
    * SerialTerminalInput  -> keyboard input over the board host serial link.
    * WebTerminalInput     -> keystrokes/events from a browser WebSocket.

- Device Input: physical buttons on the device.
    * StickInput, TEmbedInput, StampS3Input, DefaultInput -> button/encoders.

- ActionDispatcher: the central loop that reads user actions,
  dispatches them to controllers/services, and keeps the system running.

- DependencyProvider: constructs and wires together the correct Views,
  Inputs, Services, and Controllers based on the current configuration.

  A typical command follows this path:

    User types a command
            |
            v
    TerminalInput
    (read characters)
            |
            v
    ActionDispatcher
    (route command/instruction)
            |
            v
    Controller
    (Parse and validate actions)
            |
            v
    Service
    (access hardware / protocol)
            |
            v
    Controller
    (Process response)
            |
            v
    TerminalView
    (display output)

*/

void setup() {
    #if defined(DEVICE_STICKS3)
        StickS3Board board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_CARDPUTER)
        CardputerBoard board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_M5STAMPS3)
        StampS3Board board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_VISION_MASTER_T190)
        VisionMasterT190Board board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_TEMBEDS3) || defined(DEVICE_TEMBEDS3CC1101)
        TEmbedBoard board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_TDISPLAYS3)
        TDisplayS3Board board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_WAVESHARE_S3_GEEK)
        WaveshareS3GeekBoard board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_S3DEVKIT)
        S3DevKitBoard board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_UMPROS3)
        UMProS3Board board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #elif defined(DEVICE_CUSTOM)
        CustomBoard board;
        board.initialize();
        IDeviceView& deviceView = board.getDeviceView();
        IInput& deviceInput = board.getDeviceInput();
        IHostSerial& hostSerial = board.getHostSerial();
    #else
        BoardHostSerial fallbackHostSerial;
        NoScreenDeviceView fallbackDeviceView;
        DefaultInput fallbackDeviceInput;
        fallbackDeviceView.initialize();
        IDeviceView& deviceView = fallbackDeviceView;
        IInput& deviceInput = fallbackDeviceInput;
        IHostSerial& hostSerial = fallbackHostSerial;
    #endif

    // USB Adapter boot mode if set, otherwise continue to terminal type selection
    NvsService bootNvsService;
    BootModeConfigurator bootModeConfigurator(deviceView, deviceInput, bootNvsService, hostSerial);
    if (bootModeConfigurator.configure()) {
        return;
    }

    LittleFsService littleFsService;
    UtilityService utilityService;
    GlobalState& state = GlobalState::getInstance();

    //Initializing hostname from NVS
    NvsService hostnameNvsService;
    hostnameNvsService.open();
    std::string hostname = hostnameNvsService.getString(state.getNvsHostnameField(),"");
    hostnameNvsService.close();

    if (!hostname.empty()) {
        WiFi.setHostname(hostname.c_str());
    }

    // Select the terminal type
    HorizontalSelector selector(deviceView, deviceInput, utilityService);
    TerminalTypeConfigurator configurator(selector);
    TerminalTypeEnum terminalType = configurator.configure();
    std::string webIp = "0.0.0.0";

    // Configure Wi-Fi if needed
    if (terminalType == TerminalTypeEnum::WiFiClient || terminalType == TerminalTypeEnum::WiFiAp) {
        WifiTypeConfigurator wifiTypeConfigurator(deviceView, deviceInput, utilityService);
        webIp = wifiTypeConfigurator.configure(terminalType);

        if (webIp == "0.0.0.0") {
            terminalType = TerminalTypeEnum::SerialPort;
        } else {
            state.setTerminalIp(webIp);
        }
    }
    state.setTerminalMode(terminalType);

    switch (terminalType) {
        case TerminalTypeEnum::SerialPort: {
            // Serial View/Input
            SerialTerminalView serialView(hostSerial);
            SerialTerminalInput serialInput(hostSerial);

            // Baudrate
            auto baud = std::to_string(state.getSerialTerminalBaudRate());
            serialView.setBaudrate(state.getSerialTerminalBaudRate());

            // Build the provider for serial type and run the dispatcher loop
            // too big to fit on the stack anymore, allocated on the heap
            DependencyProvider* provider = new DependencyProvider(serialView, deviceView, serialInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);
            dispatcher.setup(terminalType, baud);
            dispatcher.run(); // Forever
            break;
        }
        case TerminalTypeEnum::WiFiAp:
        case TerminalTypeEnum::WiFiClient: {
            // Configure Server
            httpd_handle_t server = nullptr;
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.lru_purge_enable = true;
            config.recv_wait_timeout = 11;
            config.send_wait_timeout = 11;

            // DNS server for captive portal if AP mode
            if (terminalType == TerminalTypeEnum::WiFiAp) {
                DnsServer::configureCaptiveDns(config);
                DnsServer::startCaptiveDns(webIp);
            }

            if (httpd_start(&server, &config) != ESP_OK) {
                return;
            }

            JsonTransformer jsonTransformer;
            HttpServer httpServer(server, littleFsService, jsonTransformer);
            WebSocketServer wsServer(server);

            // Web View/Input
            WebTerminalView webView(wsServer);
            WebTerminalInput webInput(wsServer);
            deviceView.loading();
            delay(7000); // let the server begin

            // Setup routes for index, ws, captive if needed
            wsServer.setupRoutes();
            httpServer.setupRoutes();
            if (terminalType == TerminalTypeEnum::WiFiAp) {
                httpServer.setupCaptivePortalRoutes(webIp);
            }

            // Build the provider for webui type and run the dispatcher loop
            // too big to fit on the stack anymore, allocated on the heap
            DependencyProvider* provider = new DependencyProvider(webView, deviceView, webInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);

            dispatcher.setup(terminalType, webIp);
            dispatcher.run(); // Forever
            break;
        }

        #ifdef DEVICE_CARDPUTER
        case TerminalTypeEnum::Standalone:
            // Cardputer all in one
            CardputerTerminalView standaloneView; // cardputer screen as terminal
            CardputerInput standaloneInput; // cardputer keyboard for command input
            standaloneView.initialize();
            CardputerDeviceView deviceView; // used for logic analyzer only
            DefaultInput deviceInput; // the G0 button of the cardputer

            // Build the provider for cardputer standalone and run the dispatcher loop
            DependencyProvider* provider = new DependencyProvider(standaloneView, deviceView, standaloneInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);
            dispatcher.setup(terminalType, "standalone");
            dispatcher.run(); // Forever
            break;
        #endif
    }
}

void loop() {
    // Empty as all logic is handled in dispatcher
}

#endif
