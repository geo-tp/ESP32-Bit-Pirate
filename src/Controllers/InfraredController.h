// InfraredController.h
#pragma once

#include <sstream>
#include <string>
#include <algorithm>

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IDeviceView.h"
#include "Interfaces/IUtilityService.h"
#include "Interfaces/IInfraredService.h"
#include "Interfaces/ILittleFsService.h"
#include "Interfaces/II2cService.h"
#include "Models/TerminalCommand.h"
#include "Models/PinoutConfig.h"
#include "Transformers/ArgTransformer.h"
#include "Transformers/InfraredRemoteTransformer.h"
#include "Managers/UserInputManager.h"
#include "States/GlobalState.h"
#include "Interfaces/IShell.h"
#include "Shells/HelpShell.h"

class InfraredController {
public:
    // Constructor
    InfraredController(ITerminalView& view, IInput& terminalInput, IDeviceView& deviceView,
                       IUtilityService& utilityService, IInfraredService& service, ILittleFsService& littleFsService, II2cService& i2cService,
                       ArgTransformer& argTransformer, InfraredRemoteTransformer& infraredRemoteTransformer,
                       UserInputManager& userInputManager, IShell& universalRemoteShell, HelpShell& helpShell);

    // Entry point for Infraredcommand dispatch
    void handleCommand(const TerminalCommand& command);

    // Ensure infrared is properly configured
    void ensureConfigured();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    IDeviceView& deviceView;
    IUtilityService& utilityService;
    IInfraredService& infraredService;
    II2cService& i2cService;
    GlobalState& state = GlobalState::getInstance();
    ArgTransformer& argTransformer;
    InfraredRemoteTransformer& infraredRemoteTransformer;
    UserInputManager& userInputManager;
    IShell& universalRemoteShell;
    ILittleFsService& littleFsService;
    HelpShell& helpShell;
    
    bool configured = false;
    static constexpr size_t MAX_IR_FRAMES = 64;
    static constexpr size_t MAX_REPLAY_TIMINGS = 4096; // 8 KiB of uint16_t timings

    // Frames
    struct IRFrame {
        std::vector<uint16_t> timings; // raw ir timings
        uint32_t khz; // carrier frequency
        uint32_t gapUs; // silence from the previous frame's end, in microseconds
    };

    // Configure IR settings
    void handleConfig();

    // Send IR command
    void handleSend(const TerminalCommand& command);

    // Receive IR commands
    void handleReceive();
    
    // Send "device-b-gone" style power-off signals
    void handleDeviceBgone();

    // Set IR protocol
    void handleSetProtocol();

    // Universal remote shell
    void handleRemote();

    // Handle replay of IR commands
    void handleReplay(const TerminalCommand& command);
    bool recordFrames(std::vector<IRFrame>& tape);
    void playbackFrames(const std::vector<IRFrame>& tape, uint32_t replayCount);

    // Load commands from .ir files (littlefs)
    void handleLoad(const TerminalCommand& command);

    // Record raw IR frames to littlefs
    void handleRecord();

    // Send IR jamming signals
    void handleJam();

    // Show help text
    void handleHelp();
};
