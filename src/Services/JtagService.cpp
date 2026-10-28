// Porting https://github.com/Aodrulez/blueTag/ to ESP32
// Thanks to Atul Alex Cherian the original author

/* 
    [ blueTag - Hardware hacker's multi-tool based on RP2040 dev boards ]

        Inspired by JTAGulator. 

    [References & special thanks]
        https://github.com/grandideastudio/jtagulator
        https://research.kudelskisecurity.com/2019/05/16/swd-arms-alternative-to-jtag/
        https://github.com/jbentham/picoreg
        https://github.com/szymonh/SWDscan
        Yusufss4 (https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499?permalink_comment_id=4554839#gistcomment-4554839)
        Arm Debug Interface Architecture Specification (debug_interface_v5_2_architecture_specification_IHI0031F.pdf)
        
        Flashrom support : 
            https://www.flashrom.org/supported_hw/supported_prog/serprog/serprog-protocol.html
            https://github.com/stacksmashing/pico-serprog

        Openocd support  : 
            http://dangerousprototypes.com/blog/2009/10/09/bus-pirate-raw-bitbang-mode/
            http://dangerousprototypes.com/blog/2009/10/27/binary-raw-wire-mode/
            https://github.com/grandideastudio/jtagulator/blob/master/PropOCD.spin
            https://github.com/DangerousPrototypes/Bus_Pirate/blob/master/Firmware/binIO.c

        USB-to-Serial support :
            https://github.com/xxxajk/pico-uart-bridge
            https://github.com/Noltari/pico-uart-bridge

        CMSIS-DAP support :
            https://github.com/majbthrd/DapperMime
            https://github.com/raspberrypi/debugprobe
*/

#include "JtagService.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define JTAG_DELAY_US 1
#define JTAG_SCAN_SETTLE_US 5

// Set to 1 in build_flags (-DJTAG_SCAN_DEBUG=1) to trace rejected permutations.
#ifndef JTAG_SCAN_DEBUG
#define JTAG_SCAN_DEBUG 0
#endif
#define SWD_DELAY_US 5
#define LINE_RESET_CLK_CYCLES 52
#define JTAG_TO_SWD_CMD 0xE79E
#define SWD_TO_JTAG_CMD 0xE73C
#define SWDP_ACTIVATION_CODE 0x1A
#define MAX_DEVICES_LEN      32                             // Maximum number of devices allowed in a single JTAG chain
#define MIN_IR_LEN           2                              // Minimum length of instruction register per IEEE Std. 1149.1
#define MAX_IR_LEN           32                             // Maximum length of instruction register
#define MAX_IR_CHAIN_LEN     MAX_DEVICES_LEN * MAX_IR_LEN   // Maximum total length of JTAG chain w/ IR selected
#define MAX_DR_LEN           4096    

// --- JTAG ---

void JtagService::configureJtag(uint8_t tck, uint8_t tms, uint8_t tdi, uint8_t tdo, int trst) {
    _pinTCK = tck;
    _pinTMS = tms;
    _pinTDI = tdi;
    _pinTDO = tdo;
    _pinTRST = trst;

    // Preload output latches BEFORE enabling the drivers: changing pin roles
    // must not introduce an unintended rising edge on the real target TCK.
    gpio_set_level((gpio_num_t)_pinTCK, 0);
    gpio_set_level((gpio_num_t)_pinTMS, 1);
    gpio_set_level((gpio_num_t)_pinTDI, 0);
    if (trst >= 0) gpio_set_level((gpio_num_t)_pinTRST, 1);

    // Scan initialization may have enabled pull resistors on these pins.
    gpio_set_pull_mode((gpio_num_t)_pinTCK, GPIO_FLOATING);
    gpio_set_pull_mode((gpio_num_t)_pinTMS, GPIO_FLOATING);
    gpio_set_pull_mode((gpio_num_t)_pinTDI, GPIO_FLOATING);
    gpio_set_pull_mode((gpio_num_t)_pinTDO, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)_pinTDO, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)_pinTMS, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)_pinTDI, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)_pinTCK, GPIO_MODE_OUTPUT); // Enable TCK last.

    if (trst >= 0) {
        gpio_set_direction((gpio_num_t)_pinTRST, GPIO_MODE_OUTPUT);
    }
    esp_rom_delay_us(JTAG_SCAN_SETTLE_US);
}

void JtagService::tckPulse() {
    tdoRead();
}

void JtagService::tdiWrite(bool val) {
    gpio_set_level((gpio_num_t)_pinTDI, val ? 1 : 0);
}

void JtagService::tmsWrite(bool val) {
    gpio_set_level((gpio_num_t)_pinTMS, val ? 1 : 0);
}

bool JtagService::tdoRead() {
    // Bound TCK low/setup and high times even in the tight BYPASS clock loops.
    esp_rom_delay_us(JTAG_DELAY_US);
    gpio_set_level((gpio_num_t)_pinTCK, 1);
    esp_rom_delay_us(JTAG_DELAY_US);
    bool val = gpio_get_level((gpio_num_t)_pinTDO);
    gpio_set_level((gpio_num_t)_pinTCK, 0);
    return val;
}

void JtagService::restoreIdle() {
    tmsWrite(true);
    for (int i = 0; i < 8; ++i) tckPulse();
    tmsWrite(false);
    tckPulse(); // Enter Run-Test/Idle
}

void JtagService::enterShiftDR() {
    tmsWrite(true); tckPulse(); // Select DR
    tmsWrite(false); tckPulse(); // Capture DR
    tmsWrite(false); tckPulse(); // Shift DR
}

void JtagService::enterShiftIR() {
    tmsWrite(true); tckPulse(); // Select DR
    tmsWrite(true); tckPulse(); // Select IR
    tmsWrite(false); tckPulse(); // Capture IR
    tmsWrite(false); tckPulse(); // Shift IR
}

uint32_t JtagService::bitReverse(uint32_t n) {
    uint32_t r = 0;
    for (int i = 0; i < 32; ++i) {
        r <<= 1;
        r |= n & 1;
        n >>= 1;
    }
    return r;
}

uint32_t JtagService::shiftArray(uint32_t pattern, int bits) {
    uint32_t result = 0;
    for (int i = 1; i <= bits; ++i) {
        if (i == bits) tmsWrite(true);
        tdiWrite(pattern & 1);
        pattern >>= 1;
        result <<= 1;
        result |= tdoRead();
    }
    return result;
}

uint32_t JtagService::sendData(uint32_t pattern, int bits) {
    enterShiftDR();
    uint32_t out = shiftArray(pattern, bits);
    tmsWrite(true); tckPulse(); // Update DR
    tmsWrite(false); tckPulse(); // Run-Test/Idle
    return out;
}

int JtagService::detectDevices() {
    restoreIdle();
    enterShiftIR();
    tdiWrite(true);
    for (int i = 0; i < MAX_IR_CHAIN_LEN; ++i) tckPulse();
    tmsWrite(true); tckPulse(); // Exit1-IR
    tmsWrite(true); tckPulse(); // Update-IR
    tmsWrite(true); tckPulse(); // Select-DR
    tmsWrite(false); tckPulse(); // Capture-DR
    tmsWrite(false); tckPulse(); // Shift-DR

    int x;
    for(x = 0; x < MAX_DEVICES_LEN; x++)
    {
        tckPulse();
    }

    tdiWrite(false);
    for(x = 0; x < (MAX_DEVICES_LEN - 1); x++) {
        if(tdoRead() == false) {
            break;
        }
    }

    if (x > (MAX_DEVICES_LEN - 1))
    {
        x = 0;
    }

    tmsWrite(true); tckPulse();
    tmsWrite(true); tckPulse();
    tmsWrite(false); tckPulse(); // Run-Test/Idle
    return (x);
}

void JtagService::getDeviceIDs(int count, std::vector<uint32_t>& ids) {
    ids.clear();
    restoreIdle();
    enterShiftDR();
    tdiWrite(true);
    tmsWrite(false);

    for (int i = 0; i < count; ++i) {
        uint32_t id = 0;
        for (int b = 0; b < 32; ++b) {
            id <<= 1;
            id |= tdoRead();
        }
        ids.push_back(bitReverse(id));
    }
    restoreIdle();
}

uint32_t JtagService::bypassTest(int count, uint32_t pattern) {
    if (count <= 0 || count > MAX_DEVICES_LEN) return 0;
    restoreIdle();
    enterShiftIR();
    tdiWrite(true);
    for (int i = 0; i < count * MAX_IR_LEN; ++i) tckPulse();
    tmsWrite(true); tckPulse(); // Exit1 IR
    tmsWrite(true); tckPulse(); // Update IR
    tmsWrite(false); tckPulse(); // Run Test/Idle
    return bitReverse(sendData(pattern, 32 + count));
}

bool JtagService::isValidDeviceID(uint32_t id) {
    int idcode = (id & 0xFE) >> 1; // JEP106 identity is bits[7:1], not [6:0]
    int bank   = (id >> 8) & 0xF;
    return idcode > 1 && idcode <= 126 && bank <= 8;
}

void JtagService::jtagInitChannels(const std::vector<uint8_t>& pins, bool pulsePins) {
    // Pico's pull helpers disable the opposite pull; ESP-IDF's *_en helpers don't.
    for (auto pin : pins) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        if (pulsePins) {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
        } else {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
        }
    }
}

void JtagService::jtagResetChannels(const std::vector<uint8_t>& pins, int trstPin, bool pulsePins) {
    for (auto pin : pins) {
        if (trstPin >= 0 && pin == trstPin) {
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            if (pulsePins) {
                gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
            } else {
                gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
            }
        } else {
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        }
    }
}

bool JtagService::scanJtagDevice(
    const std::vector<uint8_t>& pins,
    uint8_t& outTDI, uint8_t& outTDO,
    uint8_t& outTCK, uint8_t& outTMS,
    int& outTRST,
    std::vector<uint32_t>& outDeviceIDs,
    bool pulsePins,
    void (*onProgress)(size_t, size_t)
) {
    int channelCount = pins.size();
    uint32_t tempDeviceId;
    bool volatile foundPinout = false;
    outDeviceIDs.clear();
    outTRST = -1;
    size_t progressCount = 0;
    size_t maxPermutations = channelCount * (channelCount - 1) * (channelCount - 2) * (channelCount - 3);

    for (auto tdi : pins) {
        for (auto tdo : pins) {
            if (tdi == tdo) continue;
            for (auto tck : pins) {
                if (tck == tdi || tck == tdo) continue;
                for (auto tms : pins) {
                    if (tms == tck || tms == tdo || tms == tdi) continue;

                    if (onProgress) onProgress(++progressCount, maxPermutations);

                    jtagInitChannels(pins, pulsePins);
                    if (pulsePins) {
                        for (auto ch : pins) {
                            gpio_set_direction((gpio_num_t)ch, GPIO_MODE_INPUT);
                            gpio_set_pull_mode((gpio_num_t)ch, GPIO_PULLUP_ONLY);
                            gpio_set_pull_mode((gpio_num_t)ch, GPIO_PULLDOWN_ONLY);
                        }
                    }

                    configureJtag(tck, tms, tdi, tdo);
                    int deviceCount = detectDevices();

                    if (deviceCount <= 0) {
#if JTAG_SCAN_DEBUG
                        Serial.printf("[JTAG scan] TDI=%u TDO=%u TCK=%u TMS=%u: device count=%d\n",
                                      tdi, tdo, tck, tms, deviceCount);
#endif
                        continue;
                    }
#if JTAG_SCAN_DEBUG
                    Serial.printf("[JTAG scan] TDI=%u TDO=%u TCK=%u TMS=%u: device count=%d\n",
                                  tdi, tdo, tck, tms, deviceCount);
#endif

                    uint32_t dataIn = esp_random();
                    uint32_t dataOut = bypassTest(deviceCount, dataIn);
#if JTAG_SCAN_DEBUG
                    Serial.printf("[JTAG scan] BYPASS in=0x%08lx out=0x%08lx %s\n",
                                  (unsigned long)dataIn, (unsigned long)dataOut,
                                  dataIn == dataOut ? "PASS" : "FAIL");
#endif

                    if (dataIn == dataOut) {
                        std::vector<uint32_t> ids;
                        getDeviceIDs(deviceCount, ids);
                        tempDeviceId = ids.empty() ? 0 : ids[0];

                        if (ids.empty()) {
                            Serial.println("       No device IDs read");
                            continue;
                        }

#if JTAG_SCAN_DEBUG
                        Serial.printf("[JTAG scan] IDCODE raw=0x%08lx %s\n",
                                      (unsigned long)tempDeviceId,
                                      isValidDeviceID(tempDeviceId) ? "PASS" : "FAIL");
#endif
                        if (!isValidDeviceID(tempDeviceId)) {
                            continue;
                        }

                        foundPinout = true;
                        outTDI = tdi;
                        outTDO = tdo;
                        outTCK = tck;
                        outTMS = tms;
                        outTRST = -1;
                        outDeviceIDs = ids;

                        for (auto trst : pins) {
                            if (trst == tms || trst == tck || trst == tdo || trst == tdi) continue;

                            if (onProgress) onProgress(++progressCount, maxPermutations);

                            gpio_set_direction((gpio_num_t)trst, GPIO_MODE_INPUT);
                            if (pulsePins) {
                                gpio_set_pull_mode((gpio_num_t)trst, GPIO_PULLUP_ONLY);
                            } else {
                                gpio_set_pull_mode((gpio_num_t)trst, GPIO_PULLDOWN_ONLY);
                            }

                            usleep(10);

                            std::vector<uint32_t> tmpIDs;
                            getDeviceIDs(1, tmpIDs);

                            if (!tmpIDs.empty() && tmpIDs[0] != tempDeviceId) {
                                outTRST = trst;
                                break;
                            }
                        }

                        jtagResetChannels(pins, outTRST, pulsePins);
                        return true;
                    }

                    jtagResetChannels(pins, outTRST, pulsePins);
                }
            }
        }
    }

    jtagResetChannels(pins, outTRST, pulsePins);

    if (!foundPinout && onProgress) {
        onProgress(maxPermutations, maxPermutations);
    }

    return false;
}



// --- SWD ---

void JtagService::swdDelay() {
    esp_rom_delay_us(SWD_DELAY_US);
}

void JtagService::swdClockPulse() {
    gpio_set_level((gpio_num_t)_pinSWCLK, 0);
    swdDelay();
    gpio_set_level((gpio_num_t)_pinSWCLK, 1);
    swdDelay();
}

void JtagService::swdSetReadMode() {
    gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_INPUT);
}

void JtagService::swdSetWriteMode() {
    gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_OUTPUT);
}

void JtagService::swdWriteBit(bool value) {
    gpio_set_level((gpio_num_t)_pinSWDIO, value);
    swdClockPulse();
}

void JtagService::swdWriteBits(uint32_t value, int length) {
    for (int i = 0; i < length; i++) {
        swdWriteBit((value >> i) & 1);
    }
}

bool JtagService::swdReadBit() {
    bool value = gpio_get_level((gpio_num_t)_pinSWDIO);
    swdClockPulse();
    return value;
}

bool JtagService::swdReadAck() {
    uint8_t ack = 0;
    for (int i = 0; i < 3; i++) {
        ack |= (swdReadBit() << i);
    }
    return ack == 0b001;
}

void JtagService::swdResetLineSWDJ() {
    swdSetWriteMode();
    gpio_set_level((gpio_num_t)_pinSWDIO, 1);
    for (int i = 0; i < LINE_RESET_CLK_CYCLES + 10; ++i) {
        swdClockPulse();
    }
}

void JtagService::swdToJTAG() {
    swdResetLineSWDJ();
    swdWriteBits(SWD_TO_JTAG_CMD, 16);
}

void JtagService::swdArmWakeUp() {
    swdSetWriteMode();
    gpio_set_level((gpio_num_t)_pinSWDIO, 1);
    for (int i = 0; i < 8; i++) swdClockPulse();

    const uint8_t alert[16] = {
        0x92, 0xF3, 0x09, 0x62, 0x95, 0x2D, 0x85, 0x86,
        0xE9, 0xAF, 0xDD, 0xE3, 0xA2, 0x0E, 0xBC, 0x19
    };
    for (int i = 0; i < 16; i++) swdWriteBits(alert[i], 8);

    swdWriteBits(0x00, 4); // idle
    swdWriteBits(SWDP_ACTIVATION_CODE, 8);
}

bool JtagService::swdTrySWDJ(uint32_t& idcodeOut) {
    swdArmWakeUp();
    swdResetLineSWDJ();
    swdWriteBits(JTAG_TO_SWD_CMD, 16);
    swdResetLineSWDJ();
    swdWriteBits(0x00, 4); // idle
    swdWriteBits(0xA5, 8); // read IDCODE

    swdSetReadMode();
    swdClockPulse(); // Turnaround

    bool ack = swdReadAck();
    uint32_t idcode = 0;
    if (ack) {
        for (int i = 0; i < 32; ++i) {
            idcode |= (swdReadBit() << i);
        }
        swdReadBit(); // skip parity
    }

    // End of transaction, on ack success or failure, mirrors blueTag: turnaround back to write, then 8 idle low bits
    swdClockPulse();
    swdSetWriteMode();
    swdWriteBits(0x00, 8);

    if (!ack) return false;

    idcodeOut = idcode;
    return true;
}

bool JtagService::scanSwdDevice(const std::vector<uint8_t>& pins, uint8_t& swdio, uint8_t& swclk, uint32_t& idcodeOut) {
    for (auto clk : pins) {
        for (auto io : pins) {
            if (clk == io) continue;
            _pinSWDIO = io;
            _pinSWCLK = clk;

            gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_OUTPUT);
            gpio_set_direction((gpio_num_t)_pinSWCLK, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)_pinSWDIO, 1);
            gpio_set_level((gpio_num_t)_pinSWCLK, 1);

            bool found = swdTrySWDJ(idcodeOut);
            if (found) {
                swdio = io;
                swclk = clk;
                swdToJTAG();
            }

            // Reset, mirrors blueTag: pins go back to input even after a successful find
            gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_INPUT);
            gpio_set_direction((gpio_num_t)_pinSWCLK, GPIO_MODE_INPUT);

            if (found) return true;
        }
    }
    return false;
}
