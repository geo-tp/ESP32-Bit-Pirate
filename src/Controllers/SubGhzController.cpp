#include "SubGhzController.h"

/*
Entry point for commands
*/
void SubGhzController::handleCommand(const TerminalCommand& cmd) {
    const std::string root = cmd.getRoot();

    if (root == "scan")         handleScan(cmd);
    else if (root == "sweep")        handleSweep();
    else if (root == "setfrequency") handleSetFrequency();
    else if (root == "setfreq")      handleSetFrequency();
    else if (root == "replay")       handleReplayRaw(cmd);
    else if (root == "jam")          handleJam(cmd);
    else if (root == "bruteforce")   handleBruteforce();
    else if (root == "decode")       handleDecode(cmd); // for legacy
    else if (root == "raw")          handleRaw(cmd); // in case for script usage
    else if (root == "receive")      handleReceive(cmd); // regroup decode/raw
    else if (root == "trace")        handleTrace();
    else if (root == "waterfall")    handleWaterfall();
    else if (root == "ear")          handleEar();
    else if (root == "record")       handleRecord();
    else if (root == "load")         handleLoad();
    else if (root == "send")         handleSend(cmd);
    else if (root == "config")       handleConfig();
    else                             handleHelp();
}

/*
Scan for frequencies
*/
void SubGhzController::handleScan(const TerminalCommand& cmd) {
    const auto& args = cmd.getArgs();
    auto bands = subGhzService.getSupportedBand();
    
    // Band selection
    auto bandIndex = userInputManager.readValidatedChoiceIndex("Select frequency band:", bands, 0);
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);

    // RSSI threshold / hold time selection
    int holdMs = userInputManager.readValidatedInt("Hold time per frequency (ms):", 4, 1, 5000);
    int rssiThr = userInputManager.readValidatedInt("RSSI threshold detection (dBm):", -67, -127, 0);

    // Prepare the scan
    if (!subGhzService.applyScanProfile(4.8f, 200.0f, 2 /* OOK */, true)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }

    terminalView.println("SUBGHZ Scan: Started. Hold=" + std::to_string(holdMs) +
                         " ms, Thr=" + std::to_string(rssiThr) + " dBm.... Pres [ENTER] to stop.\n");

    std::vector<int>  best(freqs.size(), -127);
    std::vector<bool> wasAbove(freqs.size(), false);
    bool stopRequested = false;

    // Scanning
    while (!stopRequested) {
        for (size_t i = 0; i < freqs.size(); ++i) {
            // User enter press
            int c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { stopRequested = true; break; }

            // Set the freq
            float f = freqs[i];
            subGhzService.tune(f);

            // Measure peak on freq for ms
            int peak = subGhzService.measurePeakRssi(holdMs);
            if (peak > best[i]) best[i] = peak;

            // Log spike if any
            if (peak >= rssiThr && !wasAbove[i]) {
                terminalView.println(" [PEAK] f=" + argTransformer.toFixed2(f) + " MHz  RSSI=" + std::to_string(peak) + " dBm");
                wasAbove[i] = true;
            } else if (peak < rssiThr - 2) {
                wasAbove[i] = false;
            }
        }
    }

    // Summary
    std::vector<size_t> idx(freqs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return best[a] > best[b]; });
    terminalView.println("\n [SCAN] Best peaks:");
    const size_t n = std::min<size_t>(5, idx.size());
    for (size_t k = 0; k < n; ++k) {
        size_t i = idx[k];
        terminalView.println("   " + argTransformer.toFixed2(freqs[i]) + " MHz  RSSI=" + std::to_string(best[i]) + " dBm");
    }

    // Ask to save best frequency if above threshold
    if (!idx.empty() && best[idx[0]] > -120) {
        auto confirm = userInputManager.readYesNo(" Save tuning to best frequency (" + argTransformer.toFixed2(freqs[idx[0]]) + " MHz)?", true);
        if (!confirm) {
            subGhzService.tune(state.getSubGhzFrequency());
            return;
        }
        subGhzService.tune(freqs[idx[0]]);
        terminalView.println(" [FREQ] Saving to config: " + argTransformer.toFixed2(freqs[idx[0]]) + " MHz\n");
        state.setSubGhzFrequency(freqs[idx[0]]);
    } else {
        subGhzService.tune(state.getSubGhzFrequency());
    }
}

/*
Set frequency
*/
void SubGhzController::handleSetFrequency() {
    terminalView.println("");
    terminalView.println("Select SubGHz Frequency:");

    // Band selection
    auto bands = subGhzService.getSupportedBand();
    std::vector<std::string> displayBands = bands;
    if (!displayBands.empty()) displayBands[0] = " Custom";
    int bandIndex = userInputManager.readValidatedChoiceIndex("Frequency band", displayBands, 0);

    // Custom frequency
    if (bandIndex == 0) {
        float mhz = userInputManager.readValidatedFloat("Custom frequency (MHz):", state.getSubGhzFrequency(), 0.0f, 1000.0f);
        state.setSubGhzFrequency(mhz);
        subGhzService.tune(mhz);
        terminalView.println("SUBGHZ: Frequency changed to " + argTransformer.toFixed2(mhz) + " MHz\n");
        return;
    }

    // Choose in predefined frequencies
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);
    int index = userInputManager.readValidatedChoiceIndex("Frequencies", freqs, 0);
    float selected = freqs[index];

    // Apply
    state.setSubGhzFrequency(selected);
    subGhzService.tune(selected);

    terminalView.println("SUBGHZ: Frequency changed to " + argTransformer.toFixed2(selected) + " MHz\n");
}

/*
Replay raw
*/
void SubGhzController::handleReplayRaw(const TerminalCommand&) {
    const float f = state.getSubGhzFrequency();

    terminalView.println("SUBGHZ Replay: Starting on " + argTransformer.toFixed2(f) + " MHz...\n");

    terminalView.println(" [ℹ️  INFORMATION]");
    terminalView.println(" The capture will include all received signals,");
    terminalView.println(" during a 3 seconds window or until 1024 symbols.\n");
    auto confirm = userInputManager.readYesNo("Start capture?", true);
    if (!confirm) return;

    // Ensure RX profile
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }
    
    // Start sniffer
    const uint8_t gdo0 = state.getSubGhzGdoPin();
    if (!subGhzService.startRawSniffer(gdo0)) {
        terminalView.println("SUBGHZ: Failed to start raw sniffer.");
        return;
    }
    
    terminalView.println("\nSUBGHZ Replay: Starting on " + argTransformer.toFixed2(f) + " MHz for 3 seconds...\n");

    // Capture 3 seconds window or 1024 symbols
    std::vector<rmt_symbol_word_t> frame = subGhzService.readRawSymbolsUntil(
        /*numSamples*/1024,
        /*timeoutMs*/3000
    );

    subGhzService.stopRawSniffer();

    if (frame.size() < 7) {
        terminalView.println("SUBGHZ Replay: Nothing captured (too short / noise).\n");
        return;
    }

    terminalView.println(" [Raw Replay]");
    terminalView.println(" Captured " + std::to_string(frame.size()) + " symbols.\n");

    // Ask to replay
    if (!userInputManager.readYesNo("Replay this RAW window now?", true)) {
        terminalView.println("SUBGHZ Replay: Discarded. Capture complete.\n");
        return;
    }

    // TX profile
    if (!subGhzService.applyRawSendProfile(f)) {
        terminalView.println("SUBGHZ: Failed to apply TX profile.");
        return;
    }

    terminalView.println("\nSUBGHZ Replay: Sending captured signals...");
    while (true) {            
        if (!subGhzService.sendRawFrame(gdo0, frame)) {
            terminalView.println("\n❌ RAW send failed.\n");
        }
        terminalView.println("\n✅ RAW window sent.\n");
        if (!userInputManager.readYesNo("Send the RAW window again?", true)) break;
    
    }

    terminalView.println("SUBGHZ Replay : Capture complete.\n");
}

/*
Replay decoded
*/
void SubGhzController::handleReplayDecoded(const TerminalCommand&) {
    const float f = state.getSubGhzFrequency();

    // Set profile to read frames
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }

    // Start sniffer
    if (!subGhzService.startRawSniffer(state.getSubGhzGdoPin())) {
        terminalView.println("SUBGHZ: Failed to start raw sniffer.");
        return;
    }

    terminalView.println("SUBGHZ Replay: Recording up to 64 frames @ " +
                         std::to_string(f) + " MHz... Press [ENTER] to stop.\n");

    std::vector<std::vector<rmt_symbol_word_t>> frames;
    frames.reserve(64);

    bool stop = false;
    while (!stop && frames.size() < 64) {
        // Cancel
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') { stop = true; break; }

        // Read frame
        auto items = subGhzService.readRawFrame();
        if (items.size() < 5) { // Most likely noise
            continue;
        }
        frames.push_back(std::move(items));
        terminalView.println(" [Frame " + std::to_string(frames.size()) + " captured]");
    }

    subGhzService.stopRawSniffer();

    if (frames.empty()) {
        terminalView.println("SUBGHZ: Nothing to replay.\n");
        return;
    }

    // Profil TX + sender
    if (!subGhzService.applyRawSendProfile(f)) {
        terminalView.println("SUBGHZ: Failed to apply TX profile.");
        return;
    }

    // Start sending frames
    terminalView.print("Captured " + std::to_string(frames.size()) + " frame(s). Replay in progress...\r\n");
    auto gdo = state.getSubGhzGdoPin();
    bool confirm = true;
    const uint16_t interFrameGapMs = 150;
    while (confirm) {
        for (size_t i = 0; i < frames.size(); ++i) {
            if (!subGhzService.sendRawFrame(gdo, frames[i])) {
                terminalView.println(" ❌ Failed at frame " + std::to_string(i + 1));
                break;
            } else {
                terminalView.println(" ✅ Sent frame " + std::to_string(i + 1) + " ... (" + std::to_string(interFrameGapMs) + "ms gap) ");
            }
            utilityService.sleepMs(interFrameGapMs);
        }
        confirm = userInputManager.readYesNo("\nReplay done. Send all the frames again?", true);
    }

    terminalView.println("SUBGHZ: Replay complete.\n");
    subGhzService.stopTxBitBang(); // ensure stopped
    subGhzService.deinitRfModule();
}

/*
Jam
*/
void SubGhzController::handleJam(const TerminalCommand&) {
    auto confirm = userInputManager.readYesNo("\nSUBGHZ Jam: This will transmit random signals. Continue?", false);
    if (!confirm) return;

    confirm = userInputManager.readYesNo("Jam multiple frequencies?", true);
    if (confirm) {
        handleBandJam();
        return;
    }

    float f = userInputManager.readValidatedFloat("Frequency to jam (MHz)", state.getSubGhzFrequency(), 0.0f, 1000.0f);
    
    // Apply TX profile with freq
    if (!subGhzService.applyRawSendProfile(f)) {
        terminalView.println("Failed to apply TX profile at " + argTransformer.toFixed2(f) + " MHz");
        return;
    }

    terminalView.println("SUBGHZ Jam: In progress @ " + argTransformer.toFixed2(f) + " MHz... Press [ENTER] to stop.");
    utilityService.sleepMs(5); // let display the message
    
    auto gdo = state.getSubGhzGdoPin();
    subGhzService.startTxBitBang();
    while (true) {
        // Stop
        auto c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Jam
        pinService.setHigh(gdo);
        utilityService.sleepUs(30);
        pinService.setLow(gdo);
    }

    subGhzService.stopTxBitBang();
    subGhzService.deinitRfModule();
    terminalView.println("SUBGHZ Jam: Stopped by user.\n");
}


/*
Band Jam
*/
void SubGhzController::handleBandJam() {
    // Select band
    auto bands = subGhzService.getSupportedBand();
    std::vector<std::string> bandsWithCustom = bands;

    int bandIndex = userInputManager.readValidatedChoiceIndex(
        "Select frequency band:", bandsWithCustom, 0
    );

    // Set band and get freqs
    subGhzService.setScanBand(bands[bandIndex]);
    auto freqs = subGhzService.getSupportedFreq(bands[bandIndex]);

    // Prompt for dwell and gap
    int dwellMs = userInputManager.readValidatedInt("Hold time per frequency (ms):", 5, 1, 10000);
    int gapUs   = userInputManager.readValidatedInt("Gap between bursts (us):",      1, 0, 500000);

    const uint8_t gdo = state.getSubGhzGdoPin();

    terminalView.println("\nSUBGHZ Jam: In progress... Press [ENTER] to stop.");
    terminalView.println("Band: " + bands[bandIndex] + ", FreqCount=" + std::to_string(freqs.size()) +
                         ", Hold=" + std::to_string(dwellMs) + " ms\n");

    bool stop = false;
    subGhzService.startTxBitBang();

    while (!stop) {
        for (size_t i = 0; i < freqs.size() && !stop; ++i) {
            // Cancel
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { stop = true; break; }

            float f = freqs[i];
            // Apply TX profile with freq
            if (!subGhzService.applyRawSendProfile(f)) {
                terminalView.println("Failed to apply TX profile at " + argTransformer.toFixed2(f) + " MHz");
                subGhzService.stopTxBitBang();
                return;
            }

            uint32_t t0 = utilityService.nowMs();
            while (!stop && (utilityService.nowMs() - t0 < static_cast<uint32_t>(dwellMs))) {
                char c2 = terminalInput.readChar();
                if (c2 == '\n' || c2 == '\r') { stop = true; break; }

                // Random burst
                if (!subGhzService.sendRandomBurst(gdo)) {
                    terminalView.println("Send failed at " + argTransformer.toFixed2(f) + " MHz");
                    break;
                }

                // Jam
                for (int i = 0; i < 64; i++) {
                    pinService.setHigh(gdo);
                    utilityService.sleepUs(30);
                    pinService.setLow(gdo);
                }

                // Gap
                int remain = gapUs;
                while (remain > 0 && !stop) {
                    char c3 = terminalInput.readChar();
                    if (c3 == '\n' || c3 == '\r') { stop = true; break; }
                    int chunk = std::min(remain, 1000);
                    utilityService.sleepUs(chunk);
                    remain -= chunk;
                }
            }
        }
    }

    subGhzService.stopTxBitBang();
    subGhzService.tune(state.getSubGhzFrequency());
    terminalView.println("SUBGHZ Jam: Stopped by user.\n");
}


/*
Raw signals reception
*/
void SubGhzController::handleRaw(const TerminalCommand&) {
    float f = state.getSubGhzFrequency();
    uint32_t count = 0;

    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }
    
    terminalView.println("SUBGHZ Raw: Frequency @ " + std::to_string(f) + " MHz... Press [ENTER] to stop\n");
    
    subGhzService.startRawSniffer(state.getSubGhzGdoPin());
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            break;
        }

        auto [line, pulseCount] = subGhzService.readRawPulses();
        if (pulseCount > 8) { // ignore too short frames, likely noise
            count += pulseCount;
            terminalView.println(line);
        }
    }
    subGhzService.stopRawSniffer();

    terminalView.println("\nSUBGHZ Raw: Stopped by user. " + std::to_string(count) + " pulses\n");
}

/*
Decoded signals reception
*/
void SubGhzController::handleDecode(const TerminalCommand&) {
    float f = state.getSubGhzFrequency();

    // Sniff profil
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }

    // Start sniffing
    if (!subGhzService.startRawSniffer(state.getSubGhzGdoPin())) {
        terminalView.println("\nFailed to start raw sniffer.\n");
        return;
    }

    terminalView.println("SUBGHZ Decode: Listening @ " +
                         std::to_string(f) + " MHz... Press [ENTER] to stop.\n");

    std::vector<rmt_symbol_word_t> frame;
    bool stop = false;
    while (!stop) {
        // Cancel
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') { stop = true; break; }

        // Read and analyze frame
        frame = subGhzService.readRawFrame();
        if (!frame.empty() && frame.size() >= 5) {
            auto result = subGhzAnalyzer.analyzeFrame(frame, subGhzService.getRxTickPerUs());
            terminalView.println(result);
        }
    }

    subGhzService.stopRawSniffer();
    terminalView.println("SUBGHZ Decode: Stopped by user.\n");
}

/*
Receive
*/
void SubGhzController::handleReceive(const TerminalCommand& cmd) {
    // Ask for raw or decoded
    auto confirm = userInputManager.readYesNo("Decode received signals to frames?", true);
    if (confirm) {
        handleDecode(cmd);
    } else {
        handleRaw(cmd);
    }
}

/*
Trace
*/
void SubGhzController::handleTrace() {
    const float f = state.getSubGhzFrequency();

    // Sniff profile
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }

    terminalView.println("\nSUBGHZ Trace: Displaying signals @ " + argTransformer.toFixed2(f) + " MHz on the ESP32 screen... Press [ENTER] to stop.\n");

    const uint8_t gdo = state.getSubGhzGdoPin();
    uint32_t sampleUs = 900;

    // Update device view
    deviceView.clear();
    deviceView.topBar(argTransformer.toFixed2(f) + " MHz", false, false);

    std::vector<uint8_t> buffer;
    buffer.reserve(240); // screen width default

    uint32_t lastPoll = utilityService.nowMs();

    // Samples loop
    while (true) {
        // Cancel
        if (utilityService.nowMs() - lastPoll >= 10) {
            lastPoll = utilityService.nowMs();
            const char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') {
                terminalView.println("SUBGHZ Trace: Stopped by user.\n");
                break;
            }
        }

        // Samples
        buffer.push_back(pinService.read(gdo));

        // Render
        if (buffer.size() >= 240) {
            buffer.resize(240);
            deviceView.drawLogicTrace(gdo, buffer, 1);
            buffer.clear();
        }

        utilityService.sleepUs(sampleUs);
    }
}

/*
Waterfall
*/
void SubGhzController::handleWaterfall()
{
    // Band selection
    auto bands = subGhzService.getSupportedBand();
    int bandIndex = userInputManager.readValidatedChoiceIndex("Select frequency band", bands, 0);
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);

    // Window ms per frequency
    uint8_t defaultWindow = bandIndex ? 100 : 32; // bandIndex 0 is all ranges
    int windowMs = userInputManager.readValidatedInt("Hold time per frequency (ms)", defaultWindow, 2, 2000);

    // Profile
    if (!subGhzService.applyScanProfile(4.8f, 200.0f, 2 /* OOK */, true)) {
        terminalView.println("SUBGHZ: Not configured. Run 'config' first.");
        return;
    }

    terminalView.println("\nSUBGHZ Waterfall: Displaying on the ESP32 screen... Press [ENTER] to stop.\n");

    float fStart = freqs.front();
    float fEnd   = freqs.back();
    const int8_t dbmThr= -70; // minimum dBm, not noise
    const int8_t dbmMin = -80; // under noise floor
    const int8_t dbmMax = -30; // strong signal
    int bestDbm = -127;
    float bestFreq = 0.0f;
    std::string title;
    size_t i = 0;

    while (true) {
        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Display peak
        if (i == 0) {
            if (bestFreq > 0.0f && bestDbm > dbmThr) {
                title = "PEAK: " + argTransformer.toFixed2(bestFreq) + "MHz " + std::to_string(bestDbm) + "dBm";
            } else {
                title = "PEAK: --";
            }

            bestDbm = -127;
            bestFreq = 0.0f;
        }

        // tune
        float f = freqs[i];
        subGhzService.tune(f);

        // measure
        int peak = subGhzService.measurePeakRssi(windowMs);
        if (peak > bestDbm) {
            bestDbm = peak;
            bestFreq = f;
        }

        // Clamp RSSI
        int v = peak;
        if (v < dbmMin) v = dbmMin;
        if (v > dbmMax) v = dbmMax;
        
        // Map to level %
        int level = (int)((int64_t)(v - dbmMin) * 100 / (dbmMax - dbmMin));
        if (level < 1) level = 1; // show progress even for weak signals

        // Draw the line for this freq
        deviceView.drawWaterfall(
            title,
            fStart,
            fEnd,
            "MHz",
            (int)i,
            (int)freqs.size(),
            level
        );

        // Next freq
        i++;
        if (i >= freqs.size()) i = 0;
    }

    subGhzService.tune(state.getSubGhzFrequency());
    terminalView.println("SUBGHZ Waterfall: Stopped by user.\n");
}

/*
Sweep
*/
void SubGhzController::handleSweep() {
    // Select band
    auto bands = subGhzService.getSupportedBand();
    int bandIndex = userInputManager.readValidatedChoiceIndex("Select frequency band:", bands, 0);
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);
    if (freqs.empty()) {
        terminalView.println("SUBGHZ Sweep: No frequencies in selected band.");
        return;
    }

    // Params
    int dwellMs  = userInputManager.readValidatedInt("Hold time per frequency (ms)", 300, 20, 5000);
    int windowMs = userInputManager.readValidatedInt("Window granularity (ms)",      20,  5,  200);
    int thrDbm   = userInputManager.readValidatedInt("RSSI threshold (dBm)",        -67, -120, 0);

    // Scan profile
    if (!subGhzService.applyScanProfile(4.8f, 200.0f, 2 /* OOK */, true)) {
        terminalView.println("SUBGHZ: Not configured. Run 'config' first.");
        return;
    }

    terminalView.println("\nSUBGHZ Sweep:" + bands[bandIndex] +
                         " | hold=" + std::to_string(dwellMs) + " ms" +
                         " | window=" + std::to_string(windowMs) + " ms" +
                         " | thr=" + std::to_string(thrDbm) + " dBm... Press [ENTER] to stop.\n");
    
    // Sweep and analyze each frequency
    bool run = true;
    while (run) {
        for (size_t i = 0; i < freqs.size() && run; ++i) {
            // Cancel
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { run = false; break; }

            // Tune
            float f = freqs[i];
            subGhzService.tune(f);

            // Analyze
            auto line = subGhzAnalyzer.analyzeFrequencyActivity(dwellMs, windowMs, thrDbm,
                // measure(windowMs)
                [&](int winMs){ return subGhzService.measurePeakRssi(winMs); },
                // shouldAbort()
                [&](){
                    char cc = terminalInput.readChar();
                    if (cc == '\n' || cc == '\r') { run = false; return true; }
                    return false;
                },
                /*neighborLeftConf=*/0.f,
                /*neighborRightConf=*/0.f
            );

            // Log result
            terminalView.println("  " + argTransformer.toFixed2(f) + " MHz  " + line);
        }
    }

    terminalView.println("\nSUBGHZ Sweep: Stopped by user.\n");
}

/*
Load
*/
void SubGhzController::handleLoad() {
    if (!littleFsService.mounted()) {
        littleFsService.begin();
    }

    // List .sub files
    auto files = littleFsService.listFiles(/*root*/ "/", ".sub");
    
    if (files.empty()) {
        terminalView.println("SUBGHZ: No .sub files found in LittleFS root ('/').\n");
        return;
    }
    
    // Select file
    files.emplace_back("Exit"); // for exit option
    terminalView.println("\n=== '.sub' files in LittleFS ===");
    int fileIndex = userInputManager.readValidatedChoiceIndex("File number", files, files.size() - 1);

    // Exit
    if (fileIndex == files.size() - 1) {
        terminalView.println("Exiting file load...\n");
        return;
    }

    std::string filename = files[fileIndex];

    // Check size
    int MAX_FILE_SIZE = 32 * 1024; // 32 KB
    auto fileSize = littleFsService.getFileSize("/" + filename);
    if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
        terminalView.println("\nSUBGHZ: File size invalid (>32KB): " + filename + " (" + std::to_string(fileSize) + " bytes)\n");
        return;
    }
    
    terminalView.println("\nSUBGHZ: Loading file '" + filename + "' (" + std::to_string(fileSize) + " bytes)...");

    // Load file
    std::string fileContent;
    fileContent.reserve(fileSize + 1);
    if (!littleFsService.readAll("/" + filename, fileContent)) {
        terminalView.println("\nSUBGHZ: Failed to read " + filename + "\n");
        return;
    }

    // Validate
    if (!subGhzTransformer.isValidSubGhzFile(fileContent)) {
        terminalView.println("\nSUBGHZ: Invalid .sub file: " + filename + "\n");
        return;
    }

    // Parse
    auto frames = subGhzTransformer.transformFromFileFormat(fileContent);
    if (frames.empty()) {
        terminalView.println("\nSUBGHZ: Failed to parse .sub file: " + filename + "\n");
        return; 
    }

    // Get frame names
    auto summaries = subGhzTransformer.extractSummaries(frames);
    summaries.push_back("Exit"); // for exit option

    while (true) {
        // Select frame to send
        terminalView.println("\n=== Commands in file '" + filename + "' ===");
        uint16_t idx = userInputManager.readValidatedChoiceIndex("Frame number", summaries, 0);

        // Exit
        if (idx == summaries.size() - 1) {
            terminalView.println("Exiting command send...\n");
            break;
        }

        // Send
        terminalView.println("\n Sending frame #" + std::to_string(idx + 1) + "...");
        const auto& cmd = frames[idx];
        if (subGhzService.send(cmd)) {
            terminalView.println(" ✅ " + summaries[idx]);
        } else {
            terminalView.println(" ❌ Send failed for frame #" + std::to_string(idx + 1));
        }
    }
}

/*
Record
*/
void SubGhzController::handleRecord() {
    constexpr size_t MIN_FREE_BYTES = 16 * 1024;

    // Mount LittleFS
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        if (!littleFsService.mounted()) {
            terminalView.println("SUBGHZ Record: LittleFS not mounted. Aborting.");
            return;
        }
        terminalView.println("SUBGHZ Record: LittleFS mounted.");
    }

    // Ensure radio configured
    const float mhz = state.getSubGhzFrequency();
    if (!subGhzService.applySniffProfile(mhz)) {
        terminalView.println("SUBGHZ Record: Not detected. Run 'config' first.");
        return;
    }

    // Start sniffer
    const uint8_t gdo0 = state.getSubGhzGdoPin();
    if (!subGhzService.startRawSniffer(gdo0)) {
        terminalView.println("SUBGHZ Record: Failed to start raw sniffer.");
        return;
    }

    terminalView.println("\nSUBGHZ Record (RAW): Capturing raw signals...\n");
    terminalView.println(" [ℹ️  INFORMATION]");
    terminalView.println(" The capture will include all received signals,");
    terminalView.println(" during a 3 seconds window or until 1024 symbols.\n");
    auto confirm = userInputManager.readYesNo("Start capture?", true);
    if (!confirm) {
        terminalView.println("SUBGHZ Record: Capture cancelled by user.\n");
        subGhzService.stopRawSniffer();
        return;
    }

    terminalView.println("\nSUBGHZ Record: Capturing for 3 seconds...\n");

    while (true) {
        // Stop with ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("SUBGHZ Record: Stopped by user.\n");
            break;
        }

        // Check free space
        size_t free = littleFsService.freeBytes();
        if (free < MIN_FREE_BYTES) {
            terminalView.println(
                "SUBGHZ Record: Not enough LittleFS space. Need >= 16KB free, have " +
                std::to_string(free) + " bytes."
            );
            break;
        }

        uint32_t tick_per_us = subGhzService.getRxTickPerUs();
        if (!tick_per_us) tick_per_us = 1;
        std::vector<rmt_symbol_word_t> items;

        // read up to 1024 symbols or 3s timeout 
        items = subGhzService.readRawSymbolsUntil(/*numSamples*/1024, /*timeoutMs*/3000);
        if (items.size() < 7) continue; // ignore noise / too short

        // Convert symbols
        std::vector<int32_t> timings = subGhzTransformer.symbolsToSignedTimings(items, tick_per_us);
        if (timings.size() < 7) continue; // ignore noise

        // Display info
        terminalView.println(" [RAW Record] ");
        terminalView.println(" Captured : " + std::to_string(items.size()));
        terminalView.println("");

        // Save or discard
        if (!userInputManager.readYesNo("Save this frame?", true)) {
            terminalView.println("SUBGHZ Record: Stopped by user.\n");
            subGhzService.readRawFrame(); // clear buffer for next frames
            break;
        }

        // Ask filename
        std::string defName = "sub_" + std::to_string(utilityService.nowMs() % 1000000);
        std::string fileBase = userInputManager.readSanitizedString("File name", defName, false);
        if (fileBase.empty()) fileBase = defName;

        std::string path = "/" + fileBase;
        if (path.size() < 5 || path.substr(path.size() - 4) != ".sub") path += ".sub";

        // Build cmd as raw in both cases
        SubGhzFileCommand cmd;
        cmd.protocol     = SubGhzProtocolEnum::RAW;
        cmd.preset       = "FuriHalSubGhzPresetOok650Async";
        cmd.frequency_hz = (uint32_t)(mhz * 1000000.0f + 0.5f);
        cmd.te_us        = 0;
        cmd.raw_timings  = std::move(timings);
        cmd.source_file  = path;

        // Serialize to .sub file format
        std::string text = subGhzTransformer.transformToFileFormat(cmd);

        // Write
        if (!littleFsService.write(path, text)) {
            terminalView.println("❌ SUBGHZ Record: Failed to write: " + path);
        } else {
            terminalView.println("✅ SUBGHZ Record: Saved file: " + path);
            terminalView.println("You can use 'load' command to replay it.\n");
        }

        break; // only one capture in this mode
    }

    subGhzService.stopRawSniffer();
}

/*
Ear
*/
void SubGhzController::handleEar() {
    // Params
    float mhz = userInputManager.readValidatedFloat("Frequency (MHz):", state.getSubGhzFrequency(), 0.0f, 1000.0f);
    int   rssiGate = userInputManager.readValidatedInt("RSSI gate (dBm):", -65, -127, 0);
    state.setSubGhzFrequency(mhz);

    // Radio init
    if (!subGhzService.applySniffProfile(mhz)) {
        terminalView.println("SUBGHZ: Not detected. Run 'config' first.");
        return;
    }
    subGhzService.tune(mhz);

    // I2S init with configured pins
    i2sService.configureOutput(
        state.getI2sBclkPin(), state.getI2sLrckPin(), state.getI2sDataPin(),
        state.getI2sSampleRate(), state.getI2sBitsPerSample(),
        state.getI2sPercentLevel()

    );

    terminalView.println("\nSUBGHZ: RSSI to Audio mapping @ " + argTransformer.toFixed2(mhz) +
                         " MHz... Press [ENTER] to stop.\n");

    terminalView.println(" [ℹ️  INFORMATION] ");
    terminalView.println(" Using I2S configured GPIOs for audio output.");
    terminalView.println(" You can set volume in the I2S mode settings.\n");

    // Mapping params
    const uint16_t fMin = 800;      // Hz for weak signals
    const uint16_t fMax = 12000;    // Hz for strong signals
    const uint16_t toneMs = 1;      // short sound
    const uint16_t refreshUs = 200; // 5 kHz update rate

    while (true) {
        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Get RSSI
        int rssi = subGhzService.measurePeakRssi(1);
        if (rssi >= rssiGate) {
            float norm = (rssi + 120.0f) / 120.0f; // normalize dbm
            if (norm < 0) norm = 0;
            if (norm > 1) norm = 1;
            uint16_t freqHz = fMin + (uint16_t)(norm * (fMax - fMin));

            // Play the tone on I2S configured pins
            i2sService.playTone(state.getI2sSampleRate(), freqHz, toneMs);
        }

        utilityService.sleepUs(refreshUs);
    }

    terminalView.println("SUBGHZ Ear: Stopped by user.\n");
}

/*
Send
*/
void SubGhzController::handleSend(const TerminalCommand& cmd) {
    // send <payload> [te]
    // TE is the base time unit from which all RF bits, syncs, and gaps are built
    std::string payloadRaw = cmd.getSubcommand();
    std::string te = cmd.getArgs();

    if (payloadRaw.empty() || !argTransformer.isValidNumber(payloadRaw)) {
        terminalView.println("Usage: send <payload> [te]");
        return;
    }

    // parse payload
    uint64_t key64 = argTransformer.parseHexOrDec64(payloadRaw);
    
    // parse TE
    uint32_t te_us = 350; // default TE in microseconds, common for many protocols
    if (!te.empty() && argTransformer.isValidNumber(te)) {
        te_us = argTransformer.parseHexOrDec32(te);
        if (te_us > 10000) te_us = 10000; // limit 
    }

    // bits count
    uint16_t bits = 0;
    if (key64 == 0) {
        bits = 1;
    } else {
        uint64_t t = key64;
        while (t) { bits++; t >>= 1; }
    }

    // Normalize bits to common lengths
    if      (bits <= 8)   bits = 8;
    else if (bits <= 12)  bits = 12;
    else if (bits <= 20)  bits = 20;
    else if (bits <= 24)  bits = 24;
    else if (bits <= 32)  bits = 32;
    else if (bits <= 36)  bits = 36;
    else if (bits <= 40)  bits = 40;
    else if (bits <= 48)  bits = 48;
    else                  bits = 64;   // max

    // Build command
    SubGhzFileCommand out;
    out.protocol     = SubGhzProtocolEnum::Princeton; // most generic
    out.preset       = "FuriHalSubGhzPresetOok650Async"; // most generic
    out.frequency_hz = (uint32_t)(state.getSubGhzFrequency() * 1000000.0f + 0.5f);
    out.te_us        = te_us;
    out.key          = key64;
    out.bits         = bits;

    terminalView.println(
        "\n[SUBGHZ Frame]\n\r"
        " Key    : " + payloadRaw + "\n\r"
        " Bits   : " + std::to_string(bits) + "\n\r"
        " Freq   : " + argTransformer.toFixed2(state.getSubGhzFrequency()) + " MHz\n\r"
        " TE     : " + std::to_string(te_us) + " us\n\r"
        " Proto  : " + SubGhzProtocolEnumMapper::toString(out.protocol) + "\n\r"
        " Preset : " + out.preset.substr(out.preset.length() - 11) + "\n"
    );

    // Send
    if (subGhzService.send(out)) {
        terminalView.println("✅ SUBGHZ: Sent.\n");
    } else {
        terminalView.println("❌ SUBGHZ: Send failed.\n");
    }
}

/*
Config CC1101
*/
bool SubGhzController::handleConfig() {
    terminalView.println("\nSubGHz Configuration:");

    bool useCardputerAdvCap = false;
    #ifdef DEVICE_CARDPUTERADV
    useCardputerAdvCap = userInputManager.readYesNo("Use Cardputer ADV CC1101 Cap?", true);
    #endif

    uint8_t sck, miso, mosi, ss, gdo0;
    if (useCardputerAdvCap) {
        // Official Cardputer ADV Cap CC1101 pinout by default
        sck = state.getSubGhzSckPin();
        miso = state.getSubGhzMisoPin();
        mosi = state.getSubGhzMosiPin();
        ss = state.getSubGhzCsPin();
        gdo0 = state.getSubGhzGdoPin();
        terminalView.println("Using Cardputer ADV CC1101 Cap pins and RF switch.");
    } else {
        auto forbidden = state.getProtectedPins();

        sck = userInputManager.readValidatedPinNumber("CC1101 SCK GPIO", state.getSubGhzSckPin(), forbidden);
        forbidden.push_back(sck);
        miso = userInputManager.readValidatedPinNumber("CC1101 MISO GPIO", state.getSubGhzMisoPin(), forbidden);
        forbidden.push_back(miso);
        mosi = userInputManager.readValidatedPinNumber("CC1101 MOSI GPIO", state.getSubGhzMosiPin(), forbidden);
        forbidden.push_back(mosi);
        ss = userInputManager.readValidatedPinNumber("CC1101 SS/CS GPIO", state.getSubGhzCsPin(), forbidden);
        forbidden.push_back(ss);
        gdo0 = userInputManager.readValidatedPinNumber("CC1101 GDO0 GPIO", state.getSubGhzGdoPin(), forbidden);
    }

    state.setSubGhzSckPin(sck);
    state.setSubGhzMisoPin(miso);
    state.setSubGhzMosiPin(mosi);
    state.setSubGhzCsPin(ss);
    state.setSubGhzGdoPin(gdo0);

    float freq = state.getSubGhzFrequency(); 
    
    // Configure
    auto isConfigured = subGhzService.configure(
        deviceView.getSharedSpiInstance(),
        sck, miso, mosi, ss,
        gdo0, freq, 10, useCardputerAdvCap
    );

    // CC1101 feedback
    if (!isConfigured) {
        configured = false;
        terminalView.println("\n ❌ Failed to detect CC1101 module. Check wiring.\n");
        return false;
    } else {

        if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
            terminalView.println("\n [ℹ️  INFORMATION] ");
            terminalView.println(" Use an antenna suited for the target frequency band.");
            terminalView.println(" For some commands, the frequency must be set precisely.");
            terminalView.println(" Example: use 433.92 MHz rather than 433 MHz for best results.\n");
        }
        
        // Apply settings
        subGhzService.tune(freq);
        subGhzService.applyScanProfile();
        terminalView.println(" ✅ CC1101 module detected and configured.");
        terminalView.println(" Use 'setfrequency' or 'scan' to change frequency.\n");
        configured = true;
        return true;
    }
}

void SubGhzController::handleBruteforce() {
    // Adapted From Bruce: https://github.com/pr3y/Bruce
    // Bruteforce attack for 12 Bit SubGHz protocols

    auto gdo0 = state.getSubGhzGdoPin();
    std::vector<std::string> protocolNames(
        std::begin(subghz_protocol_list),
        std::end(subghz_protocol_list)
    );

    // Prompt for protocol index
    auto protocolIndex = userInputManager.readValidatedChoiceIndex("\nSelect protocol to brute force:", protocolNames, 0);
    auto bruteProtocol = protocolNames[protocolIndex];

    // Freq
    float mhz = userInputManager.readValidatedFloat("Frequency (MHz):", 433.92f, 0.0f, 1000.0f);
    state.setSubGhzFrequency(mhz);

    // Profil TX + sender
    if (!subGhzService.applyRawSendProfile(mhz)) {
        terminalView.println("Failed to apply TX profile.");
        return;
    }

    c_rf_protocol protocol;
    int bits = 0;

    // Protocol selection
    if (bruteProtocol == " Nice 12 Bit") {
        protocol = protocol_nice_flo();
        bits = 12;
    } else if (bruteProtocol == " Came 12 Bit") {
        protocol = protocol_came();
        bits = 12;
    } else if (bruteProtocol == " Ansonic 12 Bit") {
        protocol = protocol_ansonic();
        bits = 12;
    } else if (bruteProtocol == " Holtek 12 Bit") {
        protocol = protocol_holtek();
        bits = 12;
    } else if (bruteProtocol == " Linear 12 Bit") {
        protocol = protocol_linear();
        bits = 12;
    } else if (bruteProtocol == " Chamberlain 12 Bit") {
        protocol = protocol_chamberlain();
        bits = 12;
    } else {
        terminalView.println("SUBGHZ BruteForce: Protocol not implemented yet.");
        return;
    }

    // Repeat
    auto bruteRepeats = userInputManager.readValidatedUint8("Number of repeats per code:", 1);
    subGhzService.startTxBitBang();

    // Send all codes
    terminalView.println("SUBGHZ BruteForce: Sending all codes for" + bruteProtocol + "... Press [ENTER] to stop.\n");
    auto count = 0;
    for (int i = 0; i < (1 << bits); ++i) {
        for (int r = 0; r < bruteRepeats; ++r) {
            for (const auto &pulse : protocol.pilot_period) { 
                subGhzService.sendRawPulse(gdo0, pulse); 
            }

            for (int j = bits - 1; j >= 0; --j) {
                bool bit = (i >> j) & 1;
                const std::vector<int> &timings = protocol.transposition_table[bit ? '1' : '0'];
                for (auto duration : timings) { 
                    subGhzService.sendRawPulse(gdo0, duration); 
                }
            }

            for (const auto &pulse : protocol.stop_bit) { subGhzService.sendRawPulse(gdo0, pulse); }
            
        }

        // Display progress
        count++;
        if (count % 100 == 0) {
            terminalView.println(" " + bruteProtocol + " @ " + argTransformer.toFixed2(mhz) + " MHz sent " + std::to_string(count) + " codes.");
        }

        // Cancel
        char cc = terminalInput.readChar();
        if (cc == '\n' || cc == '\r') {
            terminalView.println("\nSUBGHZ BruteForce: Stopped by user.\n");
            subGhzService.stopTxBitBang();
            subGhzService.deinitRfModule();

            return;
        }
    }

    subGhzService.stopTxBitBang();
    subGhzService.deinitRfModule();

    terminalView.println("\nSUBGHZ Bruteforce: Done.\n");
}

/*
Ensure SubGHz is configured
*/
void SubGhzController::ensureConfigured() {
    if (!configured) {
        configured = handleConfig();
    }
}

/*
Help
*/
void SubGhzController::handleHelp() {
    terminalView.println("\nUnknown command. Available SUBGHZ commands:");
    helpShell.run(state.getCurrentMode(), false);
}
