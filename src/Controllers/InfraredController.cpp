#include "InfraredController.h"
#include "Data/UniversalRemoteCommands.h"
#include "Transformers/InfraredCommandLimits.h"
#include <string_view>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <cstdio>

namespace {

struct IrFileEntry {
    std::string name;
    size_t begin = 0;
    size_t end = 0;
};

std::string_view trimIrLine(std::string_view value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return {};
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

// Index only the short name fields; never retain RAW data or a whole file.
// A DATA line may be 32 KiB, but its contents are irrelevant for the menu.
class IrFileIndex {
public:
    bool feed(const uint8_t* bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            const char ch = static_cast<char>(bytes[i]);
            if (!ch) return false;
            if (ch == '\n') {
                if (!finishLine()) return false;
                lineBegin_ = offset_ + 1;
            } else if (!discardLine_) {
                if (line_.size() < 256) line_ += ch;
                else {
                    const auto trimmed = trimIrLine(line_);
                    if (trimmed.substr(0, 5) == "data:" ||
                        (!trimmed.empty() && trimmed[0] == '#')) {
                        discardLine_ = true;
                    } else return false; // Reject unbounded name/metadata fields.
                }
            }
            ++offset_;
        }
        return true;
    }

    bool finish() {
        if (!line_.empty() || discardLine_) {
            if (!finishLine()) return false;
        }
        if (headerCount_ != 2) return false;
        if (!entries.empty()) entries.back().end = offset_;
        return true;
    }

    std::vector<IrFileEntry> entries;
    bool legacy = false;

private:
    bool finishLine() {
        auto line = trimIrLine(line_);
        if (headerCount_ == 0 && line.substr(0, 3) == "\xef\xbb\xbf")
            line.remove_prefix(3);
        if (!line.empty() && line[0] != '#') {
            if (headerCount_ == 0) {
                legacy = line == "Filetype: IR";
                if (!legacy && line != "Filetype: IR signals file") return false;
                ++headerCount_;
            } else if (headerCount_ == 1) {
                if (line != "Version: 1") return false;
                ++headerCount_;
            } else if (line.substr(0, 5) == "name:") {
                const auto name = trimIrLine(line.substr(5));
                if (!name.empty()) {
                    if (!entries.empty()) entries.back().end = lineBegin_;
                    if (entries.size() >= InfraredRemoteTransformer::MAX_COMMAND_COUNT)
                        return false; // Bound the filename index and menu allocations.
                    entries.push_back({std::string(name), lineBegin_, 0});
                }
            }
        }
        line_.clear();
        discardLine_ = false;
        return true;
    }

    std::string line_;
    size_t offset_ = 0;
    size_t lineBegin_ = 0;
    unsigned headerCount_ = 0;
    bool discardLine_ = false;
};

// Build only the selected command. Strip comments while streaming (large
// comments are valid in Flipper files but should not count against RAM).
class IrFileCommandSlice {
public:
    explicit IrFileCommandSlice(bool legacy) {
        text = legacy ? "Filetype: IR\nVersion: 1\n" :
                        "Filetype: IR signals file\nVersion: 1\n";
    }

    bool feed(const uint8_t* bytes, size_t n, size_t chunkOffset,
              size_t begin, size_t end) {
        if (begin >= end || end < chunkOffset) return true;
        const size_t first = begin > chunkOffset ? begin - chunkOffset : 0;
        const size_t last = std::min(n, end - chunkOffset);
        for (size_t i = first; i < last; ++i) {
            const char c = static_cast<char>(bytes[i]);
            if (!c) return false;
            if (c == '\n') {
                if (!comment_ && !atLineStart_) {
                    if (!append(c)) return false;
                }
                atLineStart_ = true;
                comment_ = false;
                continue;
            }
            if (comment_) continue;
            if (atLineStart_) {
                if (c == ' ' || c == '\t' || c == '\r') continue;
                if (c == '#') { comment_ = true; continue; }
                atLineStart_ = false;
            }
            if (!append(c)) return false;
        }
        return true;
    }

    std::string text;
private:
    bool append(char c) {
        if (text.size() >= InfraredRemoteTransformer::MAX_COMMAND_SIZE) return false;
        text += c;
        return true;
    }
    bool atLineStart_ = true;
    bool comment_ = false;
};

} // namespace

/*
Constructor
*/
InfraredController::InfraredController(
    ITerminalView&           view,
    IInput&                  terminalInput,
    IDeviceView&             deviceView,
    IUtilityService&         utilityService,
    IInfraredService&        service,
    ILittleFsService&        littleFsService,
    II2cService&             i2cService,
    ArgTransformer&          argTransformer,
    InfraredRemoteTransformer& infraredRemoteTransformer,
    UserInputManager&        userInputManager,
    IShell&                  universalRemoteShell,
    HelpShell&               helpShell
)
    : terminalView(view),
      terminalInput(terminalInput),
      deviceView(deviceView),
      utilityService(utilityService),
      infraredService(service),
      i2cService(i2cService),
      littleFsService(littleFsService),
      argTransformer(argTransformer),
      infraredRemoteTransformer(infraredRemoteTransformer),
      userInputManager(userInputManager),
      universalRemoteShell(universalRemoteShell),
      helpShell(helpShell)
{}

/*
Entry point to handle Infrared command
*/
void InfraredController::handleCommand(const TerminalCommand& command) {
    if (command.getRoot() == "config")            handleConfig();
    else if (command.getRoot() == "send")         handleSend(command);
    else if (command.getRoot() == "receive")      handleReceive();
    else if (command.getRoot() == "devicebgone")  handleDeviceBgone();
    else if (command.getRoot() == "remote")       handleRemote();
    else if (command.getRoot() == "replay")       handleReplay(command);
    else if (command.getRoot() == "record")       handleRecord();
    else if (command.getRoot() == "load")         handleLoad(command);
    else if (command.getRoot() == "jam")          handleJam();
    else if (command.getRoot() == "setprotocol")  handleSetProtocol();
    else handleHelp();
}

/*
Send
*/
void InfraredController::handleSend(const TerminalCommand& command) {
    std::istringstream iss(command.getArgs());
    std::string subStr, cmdStr, extra;
    iss >> subStr >> cmdStr;
    auto addrStr = command.getSubcommand();

    if (addrStr.empty() || subStr.empty() || cmdStr.empty()) {
        terminalView.println("Missing argument. Usage: send <device> <sub> <cmd>");
        return;
    }
    if (iss >> extra) {
        terminalView.println("Too many arguments. Usage: send <device> <sub> <cmd>");
        return;
    }

    int device, subdevice, function;
    if (!argTransformer.parseInt(addrStr, device) ||
        !argTransformer.parseInt(subStr, subdevice) ||
        !argTransformer.parseInt(cmdStr, function)) {
        terminalView.println("Invalid number format. Use decimal or hex.");
        return;
    }
    if (device < 0 || device > INT16_MAX || subdevice < -1 || subdevice > INT16_MAX ||
        function < 0 || function > INT16_MAX) {
        terminalView.println("Invalid range: device/cmd 0..32767, sub -1..32767 (protocol limits also apply).");
        return;
    }

    InfraredCommand infraredCommand;
    infraredCommand.setDevice(device);
    infraredCommand.setSubdevice(subdevice);
    infraredCommand.setFunction(function);
    infraredCommand.setProtocol(state.getInfraredProtocol());
    if (!InfraredCommandLimits::valid(infraredCommand)) {
        terminalView.println("INFRARED: Values exceed the selected protocol's field widths.");
        return;
    }

    for (int i = 0; i < 3; ++i) {
        if (!infraredService.sendInfraredCommand(infraredCommand)) {
            terminalView.println("INFRARED: Unsupported or invalid command; transmission failed.");
            return;
        }
        utilityService.sleepMs(100);
    }

    terminalView.println("IR command sent with protocol " + InfraredProtocolMapper::toString(state.getInfraredProtocol()));
}

/*
Receive
*/
void InfraredController::handleReceive() {
    bool decode = userInputManager.readYesNo("Decode infrared signal?", true);

    terminalView.println("INFRARED Receive: Waiting for signal...");
    terminalView.println("Press [ENTER] to stop.\n");

    // deviceView
    std::vector<std::string> lines;
    lines.reserve(4);
    PinoutConfig cfg;
    PinoutConfig lastCfg;
    
    infraredService.startReceiver();

    while (true) {
        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\nINFRARED Receive: Stopped by user.");
            break;
        }

        if (decode) {
            // Decode signal
            InfraredCommand cmd = infraredService.receiveInfraredCommand();
            if (cmd.getProtocol() != _RAW) {
                const std::string protocol = cmd.getDisplayProtocolName();
                const bool hasFields = cmd.getProtocol() != _IR_UNKNOWN;
                terminalView.println("");
                terminalView.println("Infrared signal received:");
                terminalView.println("  Protocol : " + protocol);
                if (hasFields) {
                    terminalView.println("  Device   : " + std::to_string(cmd.getDevice()));
                    terminalView.println("  SubDev   : " + std::to_string(cmd.getSubdevice()));
                    terminalView.println("  Command  : " + std::to_string(cmd.getFunction()));
                } else {
                    terminalView.println("  Fields   : not represented by D/S/F (use native payload or RAW)");
                }
                if (cmd.hasNativeDecode()) {
                    terminalView.println("  Bits     : " + std::to_string(cmd.getDecodedBits()));
                    if (cmd.isDecodedRepeat()) terminalView.println("  Repeat   : yes");
                    if (!cmd.getDecodedState().empty()) {
                        std::string stateHex;
                        stateHex.reserve(cmd.getDecodedState().size() * 2);
                        static const char digits[] = "0123456789ABCDEF";
                        for (uint8_t b : cmd.getDecodedState()) {
                            stateHex += digits[b >> 4];
                            stateHex += digits[b & 0x0F];
                        }
                        terminalView.println("  State    : " + stateHex +
                            (cmd.isDecodedStateTruncated() ? " (truncated)" : ""));
                    } else if (cmd.getDecodedBits() > 0 && cmd.getDecodedBits() <= 64) {
                        char hex[24];
                        std::snprintf(hex, sizeof(hex), "0x%llX",
                                      static_cast<unsigned long long>(cmd.getDecodedRawData()));
                        terminalView.println(std::string("  Raw code : ") + hex);
                    } else if (cmd.getDecodedBits() > 64) {
                        terminalView.println("  Native   : long frame (RAW capture recommended)");
                    }
                }
                terminalView.println("");
                terminalView.println("INFRARED Receive: Waiting for next signal or press [ENTER] to exit.");

                lines.clear();
                lines.push_back(protocol);
                if (hasFields) {
                    lines.push_back("Device: " + std::to_string(cmd.getDevice()));
                    lines.push_back("SubDev: " + std::to_string(cmd.getSubdevice()));
                    lines.push_back("Command: " + std::to_string(cmd.getFunction()));
                } else {
                    lines.push_back("Bits: " + std::to_string(cmd.getDecodedBits()));
                    lines.push_back("Native decode");
                    if (cmd.isDecodedRepeat()) lines.push_back("Repeat");
                }
                cfg.setMappings(lines);
                if (cfg.getMappings() != lastCfg.getMappings()) {
                    cfg.setMode("RECV");
                    lastCfg = cfg;
                    deviceView.show(cfg);
                }
                lines.clear();
            }
        } else {
            // Raw mode
            std::vector<uint16_t> timings;
            uint32_t khz = 0;

            if (infraredService.receiveRaw(timings, khz)) {
                terminalView.println("\nRAW Timings: ");

                bool mark = true;
                for (uint16_t t : timings) {
                    terminalView.print(mark ? "+" : "-");
                    terminalView.print(std::to_string(t));
                    terminalView.print(" ");
                    mark = !mark;
                }
                terminalView.println("");
            }
        }
    }

    infraredService.stopReceiver();
}

/* 
DeviceBgone
*/
void InfraredController::handleDeviceBgone() {
    terminalView.println("Sending Device-B-Gone commands... Press [ENTER] to stop");

    for (const auto& cmdStruct : universalOnOff) {

        // Convert to InfraredCommand model
        auto cmd = InfraredCommand(
            cmdStruct.proto,
            cmdStruct.device,
            cmdStruct.subdevice,
            cmdStruct.function
        );
        
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("Infrared Device-B-Gone: Interrupted by user.");
            return;
        }

        bool sent = true;
        for (int i = 0; i < 2; ++i) { // send 2x per command
            if (!infraredService.sendInfraredCommand(cmd)) { sent = false; break; }
            utilityService.sleepMs(100);
        }

        terminalView.println(
            std::string(sent ? "Sent On/Off to protocol=" : "Skipped unsupported command for protocol=") +
            InfraredProtocolMapper::toString(cmd.getProtocol()) +
            " device=" + std::to_string(cmd.getDevice()) +
            " sub=" + std::to_string(cmd.getSubdevice()) +
            " cmd=" + std::to_string(cmd.getFunction())
        );
    }

    terminalView.println("Device-B-Gone sequence completed.");
}

/*
Universal Remote
*/
void InfraredController::handleRemote() {
    universalRemoteShell.run();
}

/*
Replay
*/
void InfraredController::handleReplay(const TerminalCommand& command) {

    // Optional replay count
    uint32_t replayCount = 0; // 0 = infinite
    const std::string sub = command.getSubcommand();
    if (!sub.empty()) {
        char* end = nullptr;
        errno = 0;
        const auto parsed = std::strtoull(sub.c_str(), &end,
            sub.size() > 2 && sub[0] == '0' && (sub[1] == 'x' || sub[1] == 'X') ? 16 : 10);
        if (!argTransformer.isValidNumber(sub) || errno || *end || parsed > UINT32_MAX) {
            terminalView.println("Invalid replay count. Usage: replay [count] (0..4294967295; 0 = until ENTER).");
            return;
        }
        replayCount = static_cast<uint32_t>(parsed);
    }
    if (!command.getArgs().empty()) {
        terminalView.println("Too many arguments. Usage: replay [count]");
        return;
    }

    // Record frames
    std::vector<IRFrame> tape;
    if (!recordFrames(tape)) {
        return; // No frames captured
    }

    // Playback frames
    playbackFrames(tape, replayCount);
}

void InfraredController::handleRecord() {

    // Mount LittleFS
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        if (!littleFsService.mounted()) {
            terminalView.println("INFRARED Record: LittleFS not mounted. Aborting.");
            return;
        } else {
            terminalView.println("INFRARED Record: LittleFS mounted.");
        }
    }

    // Space check: need at least 8 kb free
    constexpr size_t MIN_FREE_BYTES = 8 * 1024;
    size_t free = littleFsService.freeBytes();
    if (free < MIN_FREE_BYTES) {
        terminalView.println(
            "INFRARED Record: Not enough LittleFS space. Need >= 8KB free, have " +
            std::to_string(free) + " bytes."
        );
        return;
    }

    // Keep only the bounded file text, not a second tape of 32-bit RAW frames.
    std::string text = "Filetype: IR signals file\nVersion: 1\n\n";
    size_t savedCommands = 0;
    const size_t maxBytes = std::min(free, InfraredRemoteTransformer::MAX_FILE_SIZE);
    std::vector<std::string> lines;
    PinoutConfig cfg;
    lines.reserve(4);

    terminalView.println("\nINFRARED Record: Waiting for frames (max 64 commands, 32 KiB file)... Press [ENTER] to stop.\n");

    infraredService.startReceiver();

    while (true) {
        // Stop ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\nINFRARED Record: Stopped.");
            break;
        }

        if (savedCommands >= MAX_IR_FRAMES) {
            terminalView.println("\nINFRARED Record: Reached maximum of 64 saved commands, stopping.\n");
            break;
        }

        std::vector<uint16_t> timings;
        uint32_t khz = 0;
        if (!infraredService.receiveRaw(timings, khz)) continue;

        terminalView.println("Infrared signal received: " + std::to_string(timings.size()) + " raw timings.");
        lines = {"Raw infrared", "Timings: " + std::to_string(timings.size()),
                 "Carrier: " + std::to_string(khz) + " kHz (assumed)"};
        cfg.setMode("RECORD");
        cfg.setMappings(lines);
        deviceView.show(cfg);

        // Save the command ?
        if (!userInputManager.readYesNo("Save this command?", true)) {
            terminalView.println("\nSkipped. Press [ENTER] to stop or wait for next signal...\n");
            infraredService.receiveRaw(timings, khz); // Clear the buffer to avoid duplicate reception during prompt
            continue;
        }

        // If yes, Ask function name
        std::string defFunc = "cmd_" + std::to_string(savedCommands + 1);
        std::string funcName = userInputManager.readSanitizedString("Enter function name", defFunc, false);
        if (funcName.empty()) funcName = defFunc;

        if (!infraredRemoteTransformer.appendRawCommand(text, funcName, timings, khz, maxBytes)) {
            terminalView.println("INFRARED Record: Frame not saved: invalid RAW data or file/space limit reached (max 32 KiB). Saving previous commands.");
            break;
        }
        ++savedCommands;

        terminalView.println("\n✅ Saved '" + funcName + "'\n");
        infraredService.receiveRaw(timings, khz); // Clear the buffer for received cmd during prompt
        terminalView.println("INFRARED Record: Waiting for next signal... Press [ENTER] to stop and save.\n");
    }

    infraredService.stopReceiver();

    if (savedCommands == 0) {
        terminalView.println("INFRARED Record: No commands saved.\n");
        return;
    }

    // Ask filename
    std::string defName = "ir_record_" + std::to_string(utilityService.nowMs() % 1000000); // court
    std::string fileBase = userInputManager.readSanitizedString("Enter file name", defName, false);
    if (fileBase.empty()) fileBase = defName;

    std::string path = "/" + fileBase;
    if (path.size() < 4 || path.substr(path.size() - 3) != ".ir") {
        path += ".ir";
    }

    // Write to LittleFS
    if (!littleFsService.write(path, text)) {
        terminalView.println("INFRARED Record: Failed to write: " + path);
        return;
    }

    terminalView.println("\n✅ INFRARED Record: Saved file: " + path);
    terminalView.println("Use 'load' command or connect to Web Terminal to get the file.\n");
}

bool InfraredController::recordFrames(std::vector<IRFrame>& tape) {
    tape.clear();
    size_t timingCount = 0;

    terminalView.println("INFRARED Replay: Recording raw IR frames (max 64 frames / 4096 timings)... Press [ENTER] to stop.\n");

    // Start the capture
    infraredService.startReceiver();
    uint32_t lastEndUs = 0;
    while (true) {
        // Stop if Enter pressed
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') break;

        // Max frames reached
        if (tape.size() >= MAX_IR_FRAMES) {
            terminalView.println("\nINFRARED Replay: Reached maximum of 64 frames, stopping recording...\n");
            break;
        }

        // Attempt to capture
        std::vector<uint16_t> timings;
        uint32_t khz = 0;
        if (infraredService.receiveRaw(timings, khz)) {
            const uint32_t now = utilityService.nowUs();
            if (timings.size() > MAX_REPLAY_TIMINGS - timingCount) {
                terminalView.println("INFRARED Replay: Reached limit of 4096 timings; last frame discarded.");
                break;
            }
            timingCount += timings.size();
            // RX reports frame completion. Remove this frame's duration from
            // end-to-end elapsed time to obtain the silence after the previous one.
            const uint32_t duration = std::accumulate(timings.begin(), timings.end(), uint32_t(0));
            const uint32_t elapsed = now - lastEndUs;
            const uint32_t gap = tape.empty() || elapsed <= duration ? 0u : elapsed - duration;
            lastEndUs = now;

            tape.push_back(IRFrame{ std::move(timings), khz, gap });
            terminalView.println(
                "  📥 Captured frame #" + std::to_string(tape.size()) +
                " (gap " + std::to_string(gap) + " us, carrier " + std::to_string(khz) + " kHz)"
            );
        }
    }
    infraredService.stopReceiver();

    // Nothing
    if (tape.empty()) {
        terminalView.println("INFRARED Replay: No frames captured. Nothing to replay.");
        return false;
    }

    return true;
}

void InfraredController::playbackFrames(const std::vector<IRFrame>& tape, uint32_t replayCount) {
    if (replayCount == 0) {
        terminalView.println("\nINFRARED Replay: Playing back with original delays. Press [ENTER] to stop.\n");
    } else {
        terminalView.println("\nINFRARED Replay: Playing back " + std::to_string(replayCount) +
                             " time(s) with original delays. Press [ENTER] to stop.\n");
    }

    // Loop through the frames and send them
    uint32_t playedLoops = 0;
    while (true) {
        if (replayCount > 0 && playedLoops >= replayCount) break;

        uint32_t previousEndUs = utilityService.nowUs();
        for (size_t i = 0; i < tape.size(); ++i) {
            const auto& f = tape[i];
            // Account for logging overhead in the interval and check ENTER even
            // for zero-gap frames, so single-frame infinite replay can stop.
            terminalView.println(
                "  📤 Sending frame #" + std::to_string(i + 1) +
                " (gap " + std::to_string(f.gapUs) + " us, carrier " + std::to_string(f.khz) + " kHz)"
            );
            while (true) {
                char c = terminalInput.readChar();
                if (c == '\r' || c == '\n') {
                    terminalView.println("\nINFRARED Replay: Stopped by user.");
                    return;
                }
                const uint32_t elapsed = utilityService.nowUs() - previousEndUs;
                if (elapsed >= f.gapUs) break;
                const uint32_t remaining = f.gapUs - elapsed;
                if (remaining >= 1000) utilityService.sleepMs(1);
                else utilityService.sleepUs(remaining);
            }

            infraredService.sendRaw(f.timings, f.khz);
            previousEndUs = utilityService.nowUs();
        }
        ++playedLoops;
    }

    terminalView.println("\nINFRARED Replay: Completed (" + std::to_string(playedLoops) + " loop(s)).");
}

/*
Set protocol
*/
void InfraredController::handleSetProtocol() {
    terminalView.println("");
    terminalView.println("Select Infrared Protocol:");

    std::vector<InfraredProtocolEnum> protocols;

    for (int i = 0; i <= static_cast<int>(_RAW); ++i) {
        InfraredProtocolEnum proto = static_cast<InfraredProtocolEnum>(i);
        std::string name = InfraredProtocolMapper::toString(proto);

        // avoid double name
        if (!name.empty() && 
            std::find_if(protocols.begin(), protocols.end(),
                [proto](InfraredProtocolEnum e) { return InfraredProtocolMapper::toString(e) == InfraredProtocolMapper::toString(proto); }) == protocols.end()) {
            protocols.push_back(proto);
            terminalView.println("  " + std::to_string(protocols.size()) + ". " + name);
        }
    }

    terminalView.println("");
    terminalView.print("Protocol Number > ");

    size_t index = 0;
    bool hasDigit = false;
    bool invalid = false;
    while (true) {
        const char c = terminalInput.handler();
        if (c == '\r' || c == '\n') {
            terminalView.println("");
            break;
        }
        if (c == CARDPUTER_SPECIAL_ARROW_DOWN || c == CARDPUTER_SPECIAL_ARROW_UP) {
            terminalView.print(std::string(1, c));
            continue;
        }
        if (c < '0' || c > '9') {
            terminalView.println("\r\nInvalid input: only digits allowed.\n");
            return;
        }
        hasDigit = true;
        // Bound the integer while reading; no unbounded string or stoi exception.
        const size_t digit = static_cast<size_t>(c - '0');
        if (index > (protocols.size() - std::min(protocols.size(), digit)) / 10)
            invalid = true;
        else if (!invalid) {
            index = index * 10 + digit;
            if (index > protocols.size()) invalid = true;
        }
        // A long sequence of digits must not exhaust memory or flood output.
        if (!invalid) terminalView.print(std::string(1, c));
    }

    if (!hasDigit) {
        terminalView.println("No input given.");
        return;
    }
    if (!invalid && index >= 1 && index <= protocols.size()) {
        const auto selected = protocols[index - 1];
        state.setInfraredProtocol(selected);
        terminalView.println("Protocol changed to " + InfraredProtocolMapper::toString(selected));
    } else {
        terminalView.println("Invalid protocol number.");
    }
}

/*
Load
*/
void InfraredController::handleLoad(TerminalCommand const& command) {
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        if (!littleFsService.mounted()) {
            terminalView.println("INFRARED Load: LittleFS not mounted. Aborting.");
            return;
        }
    }

    // Get IR files from LittleFS
    auto files = littleFsService.listFiles(/*root*/ "/", ".ir");
    if (files.empty()) {
        terminalView.println("INFRARED: No .ir files found in LittleFS root ('/').");
        return;
    }

    // Select file
    files.emplace_back("Exit"); // for exit option
    terminalView.println("\n=== '.ir' files in LittleFS ===");
    uint16_t idxFile = userInputManager.readValidatedChoiceIndex("File number", files, files.size() - 1);
    const std::string& chosen = files[idxFile];

    // Exit option
    if (chosen == "Exit") {
        terminalView.println("Exiting load command...\n");
        return;
    }

    // Check size
    auto fileSize = littleFsService.getFileSize("/" + chosen);
    if (fileSize == 0 || fileSize > InfraredRemoteTransformer::MAX_FILE_SIZE) {
        terminalView.println("\nINFRARED: File is empty or exceeds 32 KiB: " + chosen);
        return;
    }

    const std::string path = "/" + chosen;
    IrFileIndex index;
    if (!littleFsService.readChunks(path, [&](const uint8_t* chunk, size_t size) {
            return index.feed(chunk, size);
        }) || !index.finish()) {
        terminalView.println("\nINFRARED: Unrecognized .ir format or failed to read: " + chosen);
        return;
    }
    if (index.entries.empty()) {
        terminalView.println("\nINFRARED: No commands found in: " + chosen);
        return;
    }

    std::vector<std::string> cmdStrings;
    cmdStrings.reserve(index.entries.size() + 1);
    for (const auto& entry : index.entries) cmdStrings.push_back(entry.name);
    cmdStrings.push_back("Exit File");

    while (true) {
        terminalView.println("\n=== Commands in file '" + chosen + "' ===");
        const uint16_t idxCmd = userInputManager.readValidatedChoiceIndex("Command number", cmdStrings, 0);
        if (idxCmd == cmdStrings.size() - 1) {
            terminalView.println("Exiting command send...\n");
            break;
        }

        const auto& entry = index.entries[idxCmd];
        IrFileCommandSlice slice(index.legacy);
        size_t offset = 0;
        const bool read = littleFsService.readChunks(path,
            [&](const uint8_t* chunk, size_t size) {
                const bool ok = slice.feed(chunk, size, offset, entry.begin, entry.end);
                offset += size;
                return ok;
            });
        if (!read) {
            terminalView.println("INFRARED: Cannot load '" + entry.name +
                                 "': I/O error or command exceeds 8 KiB limit.");
            continue;
        }

        // Parse one command only. The input text is released before sending,
        // so RAW timings never coexist with the complete .ir file in RAM.
        auto commands = infraredRemoteTransformer.transformFromFileFormat(slice.text);
        std::string().swap(slice.text);
        if (commands.size() != 1 || !commands[0].error.empty() ||
            !infraredService.sendInfraredFileCommand(commands[0])) {
            terminalView.println("INFRARED: Cannot send '" + entry.name +
                                 "': invalid, unsupported or unavailable transmission.");
            continue;
        }
        terminalView.println("\n ✅  Sent command '" + entry.name + "' from file '" + chosen + "'");
    }
}

/*
Config
*/
void InfraredController::handleConfig() {
    terminalView.println("\nInfrared Configuration:");

    auto forbidden = state.getProtectedPins();

    uint8_t txPin = userInputManager.readValidatedPinNumber("Infrared TX GPIO", state.getInfraredTxPin(), forbidden);
    forbidden.push_back(txPin);
    uint8_t rxPin = userInputManager.readValidatedPinNumber("Infrared RX GPIO", state.getInfraredRxPin(), forbidden);
    forbidden.push_back(rxPin);
    
    state.setInfraredTxPin(txPin);
    state.setInfraredRxPin(rxPin);
    infraredService.configure(txPin, rxPin);

    // Protocol
    auto selectedProtocol = InfraredProtocolMapper::toString(state.getInfraredProtocol());
    terminalView.println("Current protocol: '" + selectedProtocol + "'");
    terminalView.println("You can change it with 'setprotocol'");
    terminalView.println("Infrared configured.\n");
}

/*
Jam
*/
void InfraredController::handleJam() {
    // Mode
    std::vector<std::string> modes = infraredService.getJamModeStrings();
    uint16_t midx = userInputManager.readValidatedChoiceIndex("Select Jam Mode", modes, 0);

    // kHz
    uint16_t khz = 38;
    if (modes[midx] == "carrier") {
        std::vector<std::string> khzChoices = infraredService.getCarrierStrings();
        uint16_t kidx = userInputManager.readValidatedChoiceIndex("Select Carrier kHz", khzChoices, 1);
        khz = (uint16_t)std::stoi(khzChoices[kidx]);
    }

    // density
    uint8_t density =  userInputManager.readValidatedInt("Density (1-20)", 10, 1, 20);

    terminalView.println("\nINFRARED Jam: Sending random signals...");
    terminalView.println("Press [ENTER] to stop.");

    uint32_t sweepIdx = 0;
    uint32_t bursts = 0;

    while (true) {
        // Stop ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\nINFRARED Jam: Stopped by user.");
            break;
        }

        infraredService.sendJam(midx, khz, sweepIdx, density);
        bursts++;
    }
}

/*
Help
*/
void InfraredController::handleHelp() {
    terminalView.println("\nUnknown command. Available INFRARED commands:");
    helpShell.run(state.getCurrentMode(), false);
}

void InfraredController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Always reconfigure before use
    uint8_t tx = state.getInfraredTxPin();
    uint8_t rx = state.getInfraredRxPin();
    infraredService.configure(tx, rx);
}
