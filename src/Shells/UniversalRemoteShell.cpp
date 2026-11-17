#include "UniversalRemoteShell.h"

UniversalRemoteShell::UniversalRemoteShell(
    ITerminalView& view,
    IInput& input,
    IUtilityService& utilityService,
    IInfraredService& irService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager
) : infraredService(irService),
    terminalView(view),
    terminalInput(input),
    utilityService(utilityService),
    argTransformer(argTransformer),
    userInputManager(userInputManager) {}

void UniversalRemoteShell::run() {
    // Remote actions
    const std::vector<std::string> actions = {
        " ⏻ ON/OFF",
        " 🔇 MUTE",
        " ▶️  PLAY",
        " ⏸️  PAUSE",
        " 🔊 VOL UP",
        " 🔉 VOL DOWN",
        " 🔼 CH UP",
        " 🔽 CH DOWN",
        " 🚪 EXIT SHELL"
    };

    terminalView.println("INFRARED: Universal IR Remote started...\n");

    while (true) {
        // Display actions
        terminalView.println("=== Universal Remote Shell ===");
        int index = userInputManager.readValidatedChoiceIndex("Select a remote action", actions, 0);
        if (index < 0 || index >= (int)actions.size()) {
            terminalView.println("Invalid selection.\n");
            continue;
        }

        // Handle exit
        if (actions[index] == " 🚪 EXIT SHELL") {
            terminalView.println("INFRARED: Exiting infrared remote shell...\n");
            break;
        }

        terminalView.println("Sending all codes for: " + actions[index] + "... Press [ENTER] to stop.\n");
        switch (index) {
        case 0: sendCommandGroup(universalOnOff,        sizeof(universalOnOff)        / sizeof(universalOnOff[0]));        break;
        case 1: sendCommandGroup(universalMute,         sizeof(universalMute)         / sizeof(universalMute[0]));         break;
        case 2: sendCommandGroup(universalPlay,         sizeof(universalPlay)         / sizeof(universalPlay[0]));         break;
        case 3: sendCommandGroup(universalPause,        sizeof(universalPause)        / sizeof(universalPause[0]));        break;
        case 4: sendCommandGroup(universalVolUp,        sizeof(universalVolUp)        / sizeof(universalVolUp[0]));        break;
        case 5: sendCommandGroup(universalVolDown,      sizeof(universalVolDown)      / sizeof(universalVolDown[0]));      break;
        case 6: sendCommandGroup(universalChannelUp,    sizeof(universalChannelUp)    / sizeof(universalChannelUp[0]));    break;
        case 7: sendCommandGroup(universalChannelDown,  sizeof(universalChannelDown)  / sizeof(universalChannelDown[0]));  break;
        }


    }
}

void UniversalRemoteShell::sendCommandGroup(const InfraredCommandStruct* group, size_t size) {
    for (size_t i = 0; i < size; ++i) {

        InfraredCommand cmd(group[i].proto, group[i].device, group[i].subdevice, group[i].function);
        if (!infraredService.sendInfraredCommand(cmd)) {
            terminalView.println("Skipped unsupported or invalid command for protocol=" +
                                 InfraredProtocolMapper::toString(cmd.getProtocol()));
            continue;
        }
        utilityService.sleepMs(100);

        // Enter press to stop
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            terminalView.println(" ⛔ Stopped by user.\n");
            return;
        }
        
        // Display sent command info
        terminalView.println(
            " ✅ Sent to protocol=" + InfraredProtocolMapper::toString(cmd.getProtocol()) +
            " device=" + std::to_string(cmd.getDevice()) +
            " sub=" + std::to_string(cmd.getSubdevice()) +
            " cmd=" + std::to_string(cmd.getFunction())
        );
    }
    terminalView.println("");
}
