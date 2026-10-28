#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Interfaces/IJtagService.h"

class FakeJtagService final : public IJtagService {
public:
    std::vector<uint8_t> lastCandidates;
    std::vector<uint32_t> jtagIds;
    uint8_t swdio = 0;
    uint8_t swclk = 0;
    uint32_t swdIdcode = 0;
    uint8_t tdi = 0;
    uint8_t tdo = 0;
    uint8_t tck = 0;
    uint8_t tms = 0;
    int trst = -1;
    bool swdFound = false;
    bool jtagFound = false;
    bool jtagFoundWithPulsing = false;
    bool lastPulsePins = false;
    std::vector<bool> pulseModes;
    uint32_t configureCalls = 0;
    uint32_t swdScanCalls = 0;
    uint32_t jtagScanCalls = 0;

    void configureJtag(uint8_t, uint8_t, uint8_t, uint8_t, int) override {
        ++configureCalls;
    }

    bool scanJtagDevice(const std::vector<uint8_t>& pins,
                        uint8_t& outTDI, uint8_t& outTDO,
                        uint8_t& outTCK, uint8_t& outTMS,
                        int& outTRST,
                        std::vector<uint32_t>& outDeviceIDs,
                        bool pulsePins,
                        void (*)(size_t, size_t)) override {
        ++jtagScanCalls;
        lastCandidates = pins;
        lastPulsePins = pulsePins;
        pulseModes.push_back(pulsePins);
        outTDI = tdi;
        outTDO = tdo;
        outTCK = tck;
        outTMS = tms;
        outTRST = trst;
        outDeviceIDs = jtagIds;
        return jtagFound || (pulsePins && jtagFoundWithPulsing);
    }

    bool scanSwdDevice(const std::vector<uint8_t>& pins,
                       uint8_t& foundIO, uint8_t& foundCLK,
                       uint32_t& idcodeOut) override {
        ++swdScanCalls;
        lastCandidates = pins;
        foundIO = swdio;
        foundCLK = swclk;
        idcodeOut = swdIdcode;
        return swdFound;
    }
};
