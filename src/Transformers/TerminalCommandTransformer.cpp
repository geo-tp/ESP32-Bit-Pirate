#include "TerminalCommandTransformer.h"
#include "Data/AutoCompleteWords.h"
#include "Enums/ModeEnum.h"
#include <sstream>
#include <cstring>

TerminalCommand TerminalCommandTransformer::transform(const std::string& raw) const {
    std::string normalized = normalizeRaw(raw);
    std::istringstream iss(normalized);

    std::string root, subcommand, args;
    iss >> root >> subcommand;
    std::getline(iss, args);

    if (!args.empty()) {
        const size_t firstNonSpace = args.find_first_not_of(' ');
        if (firstNonSpace == std::string::npos) args.clear();
        else args.erase(0, firstNonSpace);
    }

    TerminalCommand cmd;
    cmd.setRoot(root);
    cmd.setSubcommand(subcommand);
    cmd.setArgs(args);

    autoCorrectRoot(cmd);
    autoCorrectSubCommand(cmd);

    return cmd;
}

std::vector<TerminalCommand> TerminalCommandTransformer::transformMany(const std::string& raw) const {
    std::vector<TerminalCommand> result;
    constexpr size_t MAX_PIPELINE_COMMANDS = 64;

    // Trim 
    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return result;
    size_t end = raw.find_last_not_of(" \t\r\n");
    std::string line = raw.substr(start, end - start + 1);

    // Just one command
    if (line.find("||") == std::string::npos) {
        TerminalCommand cmd = transform(line);
        if (!cmd.getRoot().empty()) {
            result.push_back(cmd);
        }
        return result;
    }

    // many, split ||
    size_t pos = 0;
    while (pos < line.size()) {
        // limit
        if (result.size() >= MAX_PIPELINE_COMMANDS) {
            break;
        }

        size_t next = line.find("||", pos);

        std::string segment;
        if (next == std::string::npos) {
            segment = line.substr(pos);
            pos = line.size();
        } else {
            segment = line.substr(pos, next - pos);
            pos = next + 2;
        }

        // Trim segment
        size_t s = segment.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;

        size_t e = segment.find_last_not_of(" \t\r\n");
        segment = segment.substr(s, e - s + 1);

        if (segment.empty()) continue;

        TerminalCommand cmd = transform(segment);
        if (!cmd.getRoot().empty()) {
            result.push_back(cmd);
        }
    }

    return result;
}

std::vector<TerminalCommand> TerminalCommandTransformer::transformRepeatCommand(const std::string& raw) const {
    std::vector<TerminalCommand> result;

    if (!isRepeatCommand(raw)) {
        return result;
    }

    TerminalCommand repeatCmd = transform(raw);

    if (repeatCmd.getSubcommand().empty() || repeatCmd.getArgs().empty()) {
        return result;
    }

    char* end = nullptr;
    long value = strtol(repeatCmd.getSubcommand().c_str(), &end, 10);

    // parse error or extra chars after number
    if (end == repeatCmd.getSubcommand().c_str() || *end != '\0') {
        return result;
    }

    constexpr int MAX_REPEAT = 100;
    if (value <= 0 || value > MAX_REPEAT) {
        return result;
    }

    int count = static_cast<int>(value);
    const std::string& nestedRaw = repeatCmd.getArgs();

    std::vector<TerminalCommand> nestedCmds;

    if (isPipelineCommand(nestedRaw)) {
        nestedCmds = transformMany(nestedRaw);
    } else {
        TerminalCommand nestedCmd = transform(nestedRaw);

        // no nested repeat
        if (nestedCmd.getRoot() == "repeat") {
            return result;
        }

        nestedCmds.push_back(nestedCmd);
    }

    if (nestedCmds.empty()) {
        return result;
    }

    result.reserve(nestedCmds.size() * count);

    for (int i = 0; i < count; ++i) {
        result.insert(result.end(), nestedCmds.begin(), nestedCmds.end());
    }

    return result;
}

bool TerminalCommandTransformer::isPipelineCommand(const std::string& raw) const {
    if (raw.empty()) return false;

    const size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;

    char first = raw[start];
    if (first == '[' || first == '>' || first == '{' || first == '(') {
        return false;
    }

    return raw.find("||") != std::string::npos;
}

bool TerminalCommandTransformer::isMacroCommand(const std::string& raw) const {
    if (raw.empty()) return false;

    // Trim
    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;

    return raw[start] == '(';
}

bool TerminalCommandTransformer::isBuiltinCommand(const std::string& raw) const {
    // normalize 
    std::string normalized = normalizeRaw(raw);
    if (normalized.empty()) return false;

    // compare
    for (size_t i = 0; autoCompleteWords[i] != nullptr; ++i) {
        if (normalized == autoCompleteWords[i]) {
            return true;
        }
    }
    return false;
}

bool TerminalCommandTransformer::isRepeatCommand(const std::string& raw) const {
    std::string normalized = normalizeRaw(raw);
    if (normalized.size() < 6) return false;

    if (normalized.compare(0, 6, "repeat") != 0)
        return false;

    // "repeat" only
    if (normalized.size() == 6)
        return true;

    // "repeat <args>"
    return normalized[6] == ' ';
}

bool TerminalCommandTransformer::isGlobalCommand(const TerminalCommand& cmd) const {
    std::string root = cmd.getRoot();

    return  root == "mode"  || root == "m" || root == "l" ||
            root == "logic" || root == "logiccap" || root == "analogic" || root == "P" || root == "p" || 
            root == "system" || root == "sys" || root == "guide" || root == "man" || root == "wizard" || root == "wizreport" ||
            root == "help" || root == "h" || root == "?" || root == "hex" || root == "dec" ||
            root == "profile" || root == "delay" || root == "delayms" || root == "delayus" || 
            root == "listen" || root == "alias" || root == "repeat";
}

bool TerminalCommandTransformer::isScreenCommand(const TerminalCommand& cmd) const {
   return cmd.getRoot() == "config" || cmd.getRoot() == "setprotocol" || cmd.getRoot() == "trace"
       || cmd.getRoot() == "pullup" || cmd.getRoot() == "pulldown" || cmd.getRoot() == "reset"
       || cmd.getRoot() == "swap" || cmd.getRoot() == "logic" || cmd.getRoot() == "analogic" 
       || cmd.getRoot() == "wizard" || cmd.getRoot() == "waterfall" || cmd.getRoot() == "scan" 
       || cmd.getRoot() == "record" || cmd.getRoot() == "listen" 
       || cmd.getRoot() == "monitor" || cmd.getRoot() == "receive";
}

std::string TerminalCommandTransformer::tail(const TerminalCommand& cmd) const {
    std::string result = cmd.getSubcommand();
    if (!cmd.getArgs().empty()) {
        if (!result.empty()) result += " ";
        result += cmd.getArgs();
    }
    return result;
}

std::string TerminalCommandTransformer::normalizeRaw(const std::string& raw) const {
    // trim global
    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = raw.find_last_not_of(" \t\r\n");
    std::string s = raw.substr(start, end - start + 1);

    // extract root
    std::istringstream iss(s);
    std::string root;
    iss >> root;

    // Pullup aliases
    if (root == "P" || root == "p") {
        return root; // do nothing
    }

    // lowercase root
    for (char& c : root) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // get rest of line 
    std::string rest;
    std::getline(iss, rest); // may start with spaces

    // avoid double spaces between root and subcommand
    if (!rest.empty() && rest[0] == ' ') {
        size_t firstNonSpace = rest.find_first_not_of(' ');
        if (firstNonSpace != std::string::npos) {
            rest.erase(0, firstNonSpace - 1);
        }
    }

    return root + rest;
}

void TerminalCommandTransformer::autoCorrectRoot(TerminalCommand& cmd) const {
    std::string root = cmd.getRoot();
    if (root.empty()) return;

    auto modes = ModeEnumMapper::getProtocols();
    auto modeNames = ModeEnumMapper::getProtocolNames(modes);

    // Avoid correcting very short root
    if (root.size() < 3) return;

    // exact match against mode names
    for (const auto& s : modeNames) {
        std::string nameLower = s;
        for (char& c : nameLower) c = (char)std::tolower((unsigned char)c);
        
        // "uart" to "m uart"
        if (root == nameLower) {
            // Move typed mode into subcommand, turn root into "m"
            cmd.setRoot("m");
            cmd.setSubcommand(nameLower);
            return;
        }
    }

    // Exact match against autoCompleteWords
    for (int i = 0; autoCompleteWords[i] != nullptr; ++i) {
        const char* w = autoCompleteWords[i];
        if (std::strchr(w, ' ') != nullptr) continue; // skip "mode xxx"
        if (root == w) return;
    }

    const char* best = nullptr;
    int bestScore = 999;
    int bestCount = 0;

    for (int i = 0; autoCompleteWords[i] != nullptr; ++i) {
        const char* w = autoCompleteWords[i];
        if (std::strchr(w, ' ') != nullptr) continue;

        int score = scoreTightEditDistance(root, w);
        if (score >= 999) continue;

        if (score < bestScore) {
            bestScore = score;
            best = w;
            bestCount = 1;
            if (bestScore == 0) break;
        } else if (score == bestScore) {
            bestCount++;
        }
    }

    // Accept only very tight corrections
    if (best && bestScore <= 1 && bestCount == 1) {
        cmd.setRoot(best);
    }
}

void TerminalCommandTransformer::autoCorrectSubCommand(TerminalCommand& cmd) const {
    // Only for "mode" / "m"
    std::string root = cmd.getRoot();
    for (char& c : root) c = (char)std::tolower((unsigned char)c);

    if (root == "m") {
        cmd.setRoot("mode");
        root = "mode";
    }
    if (root != "mode") return;

    std::string sub = cmd.getSubcommand();
    if (sub.empty()) return;

    // trim
    size_t start = sub.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return;
    size_t end = sub.find_last_not_of(" \t\r\n");
    sub = sub.substr(start, end - start + 1);

    // lowercase subcmd
    for (char& c : sub) c = (char)std::tolower((unsigned char)c);

    for (int i = 0; aliases[i].from; ++i) {
        if (sub == aliases[i].from) {
            cmd.setSubcommand(aliases[i].to);
            return;
        }
    }

    // get protocol names
    auto actions = ModeEnumMapper::getProtocols();
    auto actionsStr = ModeEnumMapper::getProtocolNames(actions);

    // exact match ?
    for (const auto& s : actionsStr) {
        std::string cand = s;
        for (char& c : cand) c = (char)std::tolower((unsigned char)c);
        if (sub == cand) return;
    }

    if (sub.size() < 2) return;

    int bestIdx = -1;
    int bestScore = 999;
    int bestCount = 0;

    for (int i = 0; i < (int)actionsStr.size(); ++i) {
        std::string cand = actionsStr[(size_t)i];
        for (char& c : cand) c = (char)std::tolower((unsigned char)c);

        int score = scoreTightEditDistance(sub, cand.c_str());
        if (score >= 999) continue;

        if (score < bestScore) {
            bestScore = score;
            bestIdx = i;
            bestCount = 1;
            if (bestScore == 0) break;
        } else if (score == bestScore) {
            bestCount++;
        }
    }

    // Apply only small correction (score==1) and non ambiguous
    if (bestIdx >= 0 && bestScore == 1 && bestCount == 1) {
        cmd.setSubcommand(actionsStr[(size_t)bestIdx]);
    }
}

// 0 = exact, 1 = "tight" correction, 999 = not close
int TerminalCommandTransformer::scoreTightEditDistance(const std::string& a, const char* b) const {
    if (!b) return 999;
    const int al = (int)a.size();
    const int bl = (int)std::strlen(b);
    if (a == b) return 0;

    int diff = al - bl; if (diff < 0) diff = -diff;
    if (diff > 1) return 999;

    // Same length: 1 mismatch OR adjacent transposition
    if (al == bl) {
        int mism = 0;
        int first = -1, second = -1;

        for (int i = 0; i < al; ++i) {
            if (a[(size_t)i] != b[i]) {
                if (mism == 0) first = i;
                else second = i;
                mism++;
                if (mism > 2) break;
            }
        }

        if (mism == 1) return 1;

        if (mism == 2 && second == first + 1) {
            if (a[(size_t)first] == b[second] && a[(size_t)second] == b[first]) {
                return 1; // transposition
            }
        }
        return 999;
    }

    // Insertion/deletion: allow skipping 1 char in the longer string
    // Determine which is shorter without constructing std::string
    const bool aIsShort = (al < bl);
    int i = 0, j = 0, skips = 0;

    while ((aIsShort ? (i < al) : (i < bl)) && (aIsShort ? (j < bl) : (j < al))) {
        const char ca = aIsShort ? a[(size_t)i] : b[i];
        const char cb = aIsShort ? b[j] : a[(size_t)j];

        if (ca == cb) { i++; j++; }
        else { skips++; j++; if (skips > 1) return 999; }
    }

    return 1;
}
