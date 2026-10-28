#include "OpenOcdBusPirateAdapter.h"
#include "driver/gpio.h"
#include <cstring>

namespace {
constexpr uint8_t CMD_RESET = 0x00;
constexpr uint8_t CMD_ENTER_SPI_MODE = 0x01;
constexpr uint8_t CMD_ENTER_I2C_MODE = 0x02;
constexpr uint8_t CMD_ENTER_UART_MODE = 0x03;
constexpr uint8_t CMD_ENTER_1WIRE_MODE = 0x04;
constexpr uint8_t CMD_ENTER_RAW_WIRE_MODE = 0x05;
constexpr uint8_t CMD_ENTER_JTAG_MODE = 0x06;
constexpr uint8_t CMD_RESET_DEVICE = 0x0F;

constexpr uint8_t OOCD_UNKNOWN = 0x00;
constexpr uint8_t OOCD_PORT_MODE = 0x01;
constexpr uint8_t OOCD_FEATURE = 0x02;
constexpr uint8_t OOCD_READ_ADCS = 0x03;
constexpr uint8_t OOCD_TAP_SHIFT = 0x05;
constexpr uint8_t OOCD_ENTER = 0x06;
constexpr uint8_t OOCD_UART_SPEED = 0x07;
constexpr uint8_t OOCD_JTAG_SPEED = 0x08;

constexpr uint8_t RAW_EXIT = 0x00;
constexpr uint8_t RAW_DISPLAY_VERSION = 0x01;
constexpr uint8_t RAW_READ_BYTE = 0x06;
constexpr uint8_t RAW_READ_BIT = 0x07;
constexpr uint8_t RAW_PEEK_INPUT = 0x08;
constexpr uint8_t RAW_CLK_TICK = 0x09;
constexpr uint8_t RAW_CLK_LOW = 0x0A;
constexpr uint8_t RAW_CLK_HIGH = 0x0B;
constexpr uint8_t RAW_DATA_LOW = 0x0C;
constexpr uint8_t RAW_DATA_HIGH = 0x0D;

constexpr uint8_t RAW_BULK_TRANSFER = 0x10;
constexpr uint8_t RAW_BULK_CLK = 0x20;
constexpr uint8_t RAW_BULK_BIT = 0x30;
constexpr uint8_t RAW_PERIPH = 0x40;
constexpr uint8_t RAW_SET_SPEED = 0x60;
constexpr uint8_t RAW_CONFIG = 0x80;

constexpr uint8_t RAW_CONFIG_3V3 = 0x08;
constexpr uint8_t RAW_CONFIG_3WIRE = 0x04;
constexpr uint8_t RAW_CONFIG_LSB = 0x02;

constexpr uint16_t MAX_TAP_BITS = 0x2000;
constexpr uint16_t MAX_TAP_BYTES = MAX_TAP_BITS / 8;

constexpr uint32_t SWD_DELAY_US = 5;

inline void swdDelay() {
    delayMicroseconds(SWD_DELAY_US);
}
}

void OpenOcdBusPirateAdapter::run(const OpenOcdBusPirateConfig& adapterConfig, IInput& input, IHostSerial& hostSerialRef) {
    hostSerial = &hostSerialRef;
    config = adapterConfig;

    hostSerial->disableReboot();
    hostSerial->setRxBufferSize((MAX_TAP_BYTES * 2) + 64);
    hostSerial->begin(115200);

    configurePins();
    runBitbangMode(input);
}

void OpenOcdBusPirateAdapter::configurePins() {
    pinMode(config.tckPin, OUTPUT);
    digitalWrite(config.tckPin, LOW);

    pinMode(config.tmsPin, OUTPUT);
    digitalWrite(config.tmsPin, LOW);

    pinMode(config.tdiPin, OUTPUT);
    digitalWrite(config.tdiPin, LOW);

    pinMode(config.tdoPin, INPUT);

    pinMode(config.swclkPin, OUTPUT);
    digitalWrite(config.swclkPin, HIGH);

    pinMode(config.swdioPin, INPUT);
}

void OpenOcdBusPirateAdapter::runBitbangMode(IInput& input) {
    configurePins();
    writeString("BBIO1");

    while (true) {
        uint8_t command = readByte(input);

        if ((command & 0x80) != 0) {
            writeByte(applyBitbangPinLevels(command));
            continue;
        }

        if ((command & 0x60) == 0x40) {
            writeByte(applyBitbangPinDirections(command));
            continue;
        }

        switch (command) {
            case CMD_RESET:
                configurePins();
                writeString("BBIO1");
                break;

            case CMD_ENTER_SPI_MODE:
                writeString("SPI0");
                break;

            case CMD_ENTER_I2C_MODE:
                writeString("I2C0");
                break;

            case CMD_ENTER_UART_MODE:
                writeString("ART0");
                break;

            case CMD_ENTER_1WIRE_MODE:
                writeString("1W00");
                break;

            case CMD_ENTER_RAW_WIRE_MODE:
                writeString("RAW1");
                runSwdRawWireMode(input);
                configurePins();
                break;

            case CMD_ENTER_JTAG_MODE:
                writeString("OCD1");
                runJtagMode(input);
                configurePins();
                break;

            case CMD_RESET_DEVICE:
                configurePins();
                writeByte(1);
                writeString("BBIO1");
                break;

            default:
                writeByte(0);
                break;
        }
    }
}

void OpenOcdBusPirateAdapter::runJtagMode(IInput& input) {
    pinMode(config.tdiPin, OUTPUT);
    pinMode(config.tckPin, OUTPUT);
    pinMode(config.tmsPin, OUTPUT);
    pinMode(config.tdoPin, INPUT);

    digitalWrite(config.tdiPin, LOW);
    digitalWrite(config.tmsPin, LOW);
    digitalWrite(config.tckPin, LOW);

    while (true) {
        uint8_t command = readByte(input);

        switch (command) {
            case OOCD_UNKNOWN:
                writeString("BBIO1");
                return;

            case OOCD_ENTER:
                writeString("OCD1");
                break;

            case OOCD_READ_ADCS: {
                uint8_t response[10] = {
                    OOCD_READ_ADCS,
                    8,
                    0, 0,
                    0, 0,
                    0, 0,
                    0, 0
                };
                writeBytes(response, sizeof(response));
                break;
            }

            case OOCD_PORT_MODE:
                readByte(input);
                break;

            case OOCD_FEATURE:
                readByte(input);
                readByte(input);
                break;

            case OOCD_JTAG_SPEED:
                readByte(input);
                break;

            case OOCD_UART_SPEED: {
                uint8_t speed = readByte(input);
                readByte(input);
                readByte(input);

                uint8_t response[2] = {OOCD_UART_SPEED, speed};
                writeBytes(response, sizeof(response));
                break;
            }

            case OOCD_TAP_SHIFT: {
                uint8_t high = readByte(input);
                uint8_t low = readByte(input);

                uint16_t requestedBits = (static_cast<uint16_t>(high) << 8) | low;
                uint16_t bitCount = requestedBits;

                if (bitCount > MAX_TAP_BITS) {
                    bitCount = MAX_TAP_BITS;
                }

                uint8_t response[3] = {OOCD_TAP_SHIFT, high, low};
                writeBytes(response, sizeof(response));

                shiftJtagTap(bitCount, input);

                // If OpenOCD ever requests more than we support, drain the remaining payload.
                if (requestedBits > MAX_TAP_BITS) {
                    uint16_t requestedBytes = (requestedBits + 7) / 8;
                    uint16_t consumedBytes = (MAX_TAP_BITS + 7) / 8;

                    for (uint16_t i = consumedBytes; i < requestedBytes; ++i) {
                        readByte(input); // TDI
                        readByte(input); // TMS
                    }
                }

                break;
            }

            default:
                writeByte(OOCD_UNKNOWN);
                break;
        }
    }
}

void OpenOcdBusPirateAdapter::runSwdRawWireMode(IInput& input) {
    pinMode(config.swclkPin, OUTPUT);
    pinMode(config.swdioPin, INPUT);

    swdLsbFirst = true;
    swdNeedsTurnaround = false;

    // Match the working JtagService SWD behavior:
    // clock idle high, pulse low->high, sample before pulse.
    swdClockHigh();
    swdReleaseData();

    while (true) {
        uint8_t command = readByte(input);

        switch (command) {
            case RAW_EXIT:
                swdReleaseData();
                swdClockHigh();
                writeString("BBIO1");
                return;

            case RAW_DISPLAY_VERSION:
                writeString("RAW1");
                break;

            case RAW_READ_BYTE:
                writeByte(swdReadByte());
                break;

            case RAW_READ_BIT:
                writeByte(swdReadBitClocked());
                break;

            case RAW_PEEK_INPUT:
                swdReleaseData();
                writeByte(gpio_get_level(static_cast<gpio_num_t>(config.swdioPin)) ? 1 : 0);
                break;

            case RAW_CLK_TICK:
                swdReleaseData();
                swdClockPulse();
                writeByte(1);
                break;

            case RAW_CLK_LOW:
                swdClockLow();
                writeByte(1);
                break;

            case RAW_CLK_HIGH:
                swdClockHigh();
                writeByte(1);
                break;

            case RAW_DATA_LOW:
                swdDataLow();
                writeByte(1);
                break;

            case RAW_DATA_HIGH:
                swdDataHigh();
                writeByte(1);
                break;

            default: {
                uint8_t highNibble = command & 0xF0;

                if (highNibble == RAW_BULK_TRANSFER) {
                    uint8_t count = (command & 0x0F) + 1;
                    uint8_t data[16];

                    for (uint8_t i = 0; i < count; ++i) {
                        data[i] = readByte(input);
                    }

                    writeByte(1);

                    for (uint8_t i = 0; i < count; ++i) {
                        writeByte(swdWriteByte(data[i], count == 1));
                    }

                } else if (highNibble == RAW_BULK_CLK) {
                    uint8_t count = (command & 0x0F) + 1;

                    swdReleaseData();
                    for (uint8_t i = 0; i < count; ++i) {
                        swdClockPulse();
                    }

                    writeByte(1);

                } else if (highNibble == RAW_BULK_BIT) {
                    uint8_t bits = (command & 0x0F) + 1;
                    uint8_t value = readByte(input);

                    if (!swdLsbFirst) {
                        value = reverseByte(value);
                    }

                    for (uint8_t i = 0; i < bits; ++i) {
                        swdWriteBit((value >> i) & 1);
                    }

                    // Bulk bit is used as an output operation. Release afterwards so reads can work
                    swdReleaseData();
                    writeByte(1);

                } else if (highNibble == RAW_PERIPH) {
                    if (command & 0x01) {
                        gpio_pullup_en(static_cast<gpio_num_t>(config.swdioPin));
                    } else {
                        gpio_pullup_dis(static_cast<gpio_num_t>(config.swdioPin));
                    }
                    writeByte(1);

                } else if (highNibble == RAW_SET_SPEED) {
                    // OpenOCD expects ACK. Actual speed is fixed by SWD_DELAY_US.
                    writeByte(1);

                } else if (highNibble == RAW_CONFIG) {
                    swdLsbFirst = (command & RAW_CONFIG_LSB) != 0;

                    if (command & RAW_CONFIG_3V3) {
                        gpio_pullup_en(static_cast<gpio_num_t>(config.swdioPin));
                    } else {
                        gpio_pullup_dis(static_cast<gpio_num_t>(config.swdioPin));
                    }

                    (void)(command & RAW_CONFIG_3WIRE);

                    swdNeedsTurnaround = false;
                    swdClockHigh();
                    swdReleaseData();
                    writeByte(1);

                } else {
                    writeByte(1);
                }

                break;
            }
        }
    }
}

uint8_t OpenOcdBusPirateAdapter::readByte(IInput& input) {
    while (hostSerial->available() <= 0) {
        if (inputRequestedReset(input)) {
            ESP.restart();
        }
    }

    return static_cast<uint8_t>(hostSerial->read());
}

void OpenOcdBusPirateAdapter::writeByte(uint8_t value) {
    hostSerial->write(value);
}

void OpenOcdBusPirateAdapter::writeBytes(const uint8_t* data, size_t length) {
    hostSerial->write(data, length);
}

void OpenOcdBusPirateAdapter::writeString(const char* value) {
    hostSerial->write(reinterpret_cast<const uint8_t*>(value), strlen(value));
}

bool OpenOcdBusPirateAdapter::inputRequestedReset(IInput& input) {
    return input.readChar() != KEY_NONE;
}

uint8_t OpenOcdBusPirateAdapter::readJtagTdo() {
    gpio_set_level(static_cast<gpio_num_t>(config.tckPin), 1);
    __asm__ __volatile__("nop");

    uint8_t value = gpio_get_level(static_cast<gpio_num_t>(config.tdoPin)) ? 1 : 0;

    gpio_set_level(static_cast<gpio_num_t>(config.tckPin), 0);
    return value;
}

void OpenOcdBusPirateAdapter::writeJtagTdi(bool value) {
    gpio_set_level(static_cast<gpio_num_t>(config.tdiPin), value ? 1 : 0);
}

void OpenOcdBusPirateAdapter::writeJtagTms(bool value) {
    gpio_set_level(static_cast<gpio_num_t>(config.tmsPin), value ? 1 : 0);
}

void OpenOcdBusPirateAdapter::shiftJtagTap(uint16_t bitCount, IInput& input) {
    uint16_t byteCount = (bitCount + 7) / 8;

    uint8_t tdi[MAX_TAP_BYTES] = {};
    uint8_t tms[MAX_TAP_BYTES] = {};

    for (uint16_t i = 0; i < byteCount; ++i) {
        tdi[i] = readByte(input);
        tms[i] = readByte(input);
    }

    uint8_t outputByte = 0;
    uint8_t outputBit = 0;

    for (uint16_t bit = 0; bit < bitCount; ++bit) {
        uint16_t index = bit / 8;
        uint8_t mask = 1 << (bit % 8);

        writeJtagTdi((tdi[index] & mask) != 0);
        writeJtagTms((tms[index] & mask) != 0);

        if (readJtagTdo()) {
            outputByte |= (1 << outputBit);
        }

        outputBit++;

        if (outputBit == 8 || bit == bitCount - 1) {
            writeByte(outputByte);
            outputByte = 0;
            outputBit = 0;
        }
    }

    writeJtagTdi(false);
    writeJtagTms(false);
}

void OpenOcdBusPirateAdapter::swdClockLow() {
    gpio_set_level(static_cast<gpio_num_t>(config.swclkPin), 0);
}

void OpenOcdBusPirateAdapter::swdClockHigh() {
    gpio_set_level(static_cast<gpio_num_t>(config.swclkPin), 1);
}

void OpenOcdBusPirateAdapter::swdClockPulse() {
    swdClockLow();
    swdDelay();
    swdClockHigh();
    swdDelay();
}

void OpenOcdBusPirateAdapter::swdClockTick() {
    swdClockPulse();
}

void OpenOcdBusPirateAdapter::swdDataLow() {
    pinMode(config.swdioPin, OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(config.swdioPin), 0);
}

void OpenOcdBusPirateAdapter::swdDataHigh() {
    pinMode(config.swdioPin, OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(config.swdioPin), 1);
}

void OpenOcdBusPirateAdapter::swdReleaseData() {
    pinMode(config.swdioPin, INPUT);
}

void OpenOcdBusPirateAdapter::swdWriteBit(uint8_t value) {
    pinMode(config.swdioPin, OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(config.swdioPin), value ? 1 : 0);

    swdClockPulse();
}

uint8_t OpenOcdBusPirateAdapter::swdReadBit() {
    swdReleaseData();
    return gpio_get_level(static_cast<gpio_num_t>(config.swdioPin)) ? 1 : 0;
}

uint8_t OpenOcdBusPirateAdapter::swdReadBitClocked() {
    // Genuine Bus Pirate raw-wire semantics: clock first, then sample.
    // OpenOCD's buspirate.c counts turnaround clocks assuming the bit is
    // sampled AFTER the rising edge (the target shifts out on that edge).
    // Sampling before the edge, plus the auto-turnaround heuristic, leaves
    // one extra released clock after every read (a stray start bit that
    // trips a protocol error) and one extra clock before write data
    // (write data shifted by one bit).
    swdReleaseData();
    swdNeedsTurnaround = false;
    swdClockPulse();

    return swdReadBit();
}

uint8_t OpenOcdBusPirateAdapter::swdReadByte() {
    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; ++i) {
        value |= swdReadBitClocked() << i;
    }

    return swdLsbFirst ? value : reverseByte(value);
}

uint8_t OpenOcdBusPirateAdapter::swdWriteByte(uint8_t value) {
    return swdWriteByte(value, true);
}

uint8_t OpenOcdBusPirateAdapter::swdWriteByte(uint8_t value, bool mayBeSwdRequest) {
    uint8_t originalValue = value;

    if (!swdLsbFirst) {
        value = reverseByte(value);
    }

    for (uint8_t i = 0; i < 8; ++i) {
        swdWriteBit((value >> i) & 1);
    }

    swdReleaseData();

    if (mayBeSwdRequest && isSwdRequestByte(originalValue)) {
        swdNeedsTurnaround = true;
    }

    return 0;
}

bool OpenOcdBusPirateAdapter::isSwdRequestByte(uint8_t value) {
    return (value & 0x81) == 0x81 && (value & 0x40) == 0;
}

uint8_t OpenOcdBusPirateAdapter::reverseByte(uint8_t value) {
    value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
    value = ((value & 0xAA) >> 1) | ((value & 0x55) << 1);

    return (value >> 4) | (value << 4);
}

uint8_t OpenOcdBusPirateAdapter::applyBitbangPinDirections(uint8_t command) {
    pinMode(config.tdiPin, (command & 0x08) ? OUTPUT : INPUT);
    pinMode(config.tckPin, (command & 0x04) ? OUTPUT : INPUT);
    pinMode(config.tdoPin, (command & 0x02) ? OUTPUT : INPUT);
    pinMode(config.tmsPin, (command & 0x01) ? OUTPUT : INPUT);

    return readBitbangPins();
}

uint8_t OpenOcdBusPirateAdapter::applyBitbangPinLevels(uint8_t command) {
    gpio_set_level(static_cast<gpio_num_t>(config.tdiPin), (command & 0x08) ? 1 : 0);
    gpio_set_level(static_cast<gpio_num_t>(config.tckPin), (command & 0x04) ? 1 : 0);
    gpio_set_level(static_cast<gpio_num_t>(config.tdoPin), (command & 0x02) ? 1 : 0);
    gpio_set_level(static_cast<gpio_num_t>(config.tmsPin), (command & 0x01) ? 1 : 0);

    return readBitbangPins();
}

uint8_t OpenOcdBusPirateAdapter::readBitbangPins() {
    uint8_t value = 0;

    if (gpio_get_level(static_cast<gpio_num_t>(config.tdiPin)) != 0) {
        value |= 0x08;
    }

    if (gpio_get_level(static_cast<gpio_num_t>(config.tckPin)) != 0) {
        value |= 0x04;
    }

    if (gpio_get_level(static_cast<gpio_num_t>(config.tdoPin)) != 0) {
        value |= 0x02;
    }

    if (gpio_get_level(static_cast<gpio_num_t>(config.tmsPin)) != 0) {
        value |= 0x01;
    }

    return value;
}