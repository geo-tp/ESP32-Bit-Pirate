#include "FlashromSerprogAdapter.h"
#include <USBCDC.h>
#include "driver/gpio.h"
#include "States/GlobalState.h"
#include <algorithm>

namespace {
    constexpr uint8_t ACK = 0x06;
    constexpr uint8_t NAK = 0x15;
    constexpr uint8_t BUS_SPI = 0x08;
    constexpr uint32_t DEFAULT_SPI_FREQUENCY = 8000000;
    constexpr uint32_t MAX_SPI_FREQUENCY = 40000000;
    constexpr uint32_t SERPROG_MAX_TRANSFER = 0xFFFFFF;
    constexpr size_t SERIAL_BUFFER_SIZE = 65535;
    constexpr uint32_t READ_TIMEOUT_MS = 1000;
    constexpr uint32_t TX_TIMEOUT_MS = 5000;
    constexpr uint32_t TRANSFER_ABORT_CHECK_INTERVAL = 64;

    enum SerprogCommand : uint8_t {
        CMD_NOP = 0x00,
        CMD_Q_IFACE = 0x01,
        CMD_Q_CMDMAP = 0x02,
        CMD_Q_PGMNAME = 0x03,
        CMD_Q_SERBUF = 0x04,
        CMD_Q_BUSTYPE = 0x05,
        CMD_Q_WRNMAXLEN = 0x08,
        CMD_SYNCNOP = 0x10,
        CMD_Q_RDNMAXLEN = 0x11,
        CMD_S_BUSTYPE = 0x12,
        CMD_O_SPIOP = 0x13,
        CMD_S_SPI_FREQ = 0x14,
        CMD_S_PIN_STATE = 0x15,
        CMD_S_SPI_CS = 0x16,
        CMD_S_SPI_MODE = 0x17,
        CMD_S_CS_MODE = 0x18,
    };
}

void FlashromSerprogAdapter::run(const FlashromSerprogConfig& adapterConfig, IInput& input, IHostSerial& hostSerialRef) {
    hostSerial = &hostSerialRef;
    config = adapterConfig;
    pinsEnabled = false;
    transactionActive = false;
    cdcConnected = false;
    csMode = 0;

    const auto& protectedPins = GlobalState::getInstance().getProtectedPins();
    const auto validControlPin = [&](int8_t pin) {
        return pin >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(pin) &&
               pin != config.csPin && pin != config.sckPin &&
               pin != config.mosiPin && pin != config.misoPin &&
               std::find(protectedPins.begin(), protectedPins.end(), pin) == protectedPins.end();
    };
    config.wpPin = validControlPin(config.wpPin) ? config.wpPin : -1;
    config.holdPin = validControlPin(config.holdPin) && config.holdPin != config.wpPin ? config.holdPin : -1;

    if (config.frequency == 0) {
        config.frequency = DEFAULT_SPI_FREQUENCY;
    }

    hostSerial->disableReboot();
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(TX_TIMEOUT_MS);
    Serial.onEvent(onUsbEvent);
#endif
    hostSerial->setRxBufferSize(SERIAL_BUFFER_SIZE);
    hostSerial->begin(115200);
#if !ARDUINO_USB_CDC_ON_BOOT
        cdcConnected = true;
#endif

    initializeSpi();
    setPinDrivers(true);

    constexpr uint32_t INPUT_POLL_INTERVAL_MS = 25;
    uint32_t lastInputPollMs = 0;

    while (true) {
        // Priority: serprog/flashrom traffic first.
        while (hostSerial->available() > 0) {
            cdcConnected = true;
            handleCommand(static_cast<uint8_t>(hostSerial->read()), input);
        }

        // Poll device input only periodically so it does not slow down serprog
        uint32_t now = millis();
        if ((uint32_t)(now - lastInputPollMs) >= INPUT_POLL_INTERVAL_MS) {
            lastInputPollMs = now;

            if (inputRequestedReset(input)) {
                setPinDrivers(false);
                hostSerial->flush();
                ESP.restart();
            }
        }

        yield();
    }
}

void FlashromSerprogAdapter::initializeSpi() {
    transactionActive = false;
    // Preload HIGH before enabling outputs, keeping CS and optional WP/HOLD inactive.
    gpio_set_level(static_cast<gpio_num_t>(config.csPin), HIGH);
    pinMode(config.csPin, OUTPUT);
    spi.begin(config.sckPin, config.misoPin, config.mosiPin, config.csPin);
    for (int8_t pin : {config.wpPin, config.holdPin}) {
        if (pin < 0) continue;
        gpio_set_level(static_cast<gpio_num_t>(pin), HIGH);
        pinMode(pin, OUTPUT);
    }
}

void FlashromSerprogAdapter::setPinDrivers(bool enabled) {
    resetSpiBusState();
    pinsEnabled = enabled;
    if (enabled) {
        spi.end();
        initializeSpi();
        return;
    }

    spi.end();
    digitalWrite(config.csPin, HIGH);
    pinMode(config.csPin, INPUT);
    pinMode(config.sckPin, INPUT);
    pinMode(config.mosiPin, INPUT);
    pinMode(config.misoPin, INPUT);
    for (int8_t pin : {config.wpPin, config.holdPin}) {
        if (pin >= 0) pinMode(pin, INPUT);
    }
}

void FlashromSerprogAdapter::setChipSelectAsserted(bool asserted) {
    digitalWrite(config.csPin, asserted ? LOW : HIGH);
}

void FlashromSerprogAdapter::resetSpiBusState() {
    csMode = 0;
    setChipSelectAsserted(false);
    if (transactionActive) {
        spi.endTransaction();
        transactionActive = false;
    }
}

void FlashromSerprogAdapter::purgeInput() {
    while (hostSerial->available() > 0) {
        hostSerial->read();
    }
}

void FlashromSerprogAdapter::applyCsModeBeforeTransfer() {
    if (csMode == 0 || csMode == 1) {
        setChipSelectAsserted(true);
    } else {
        setChipSelectAsserted(false);
    }
}

void FlashromSerprogAdapter::applyCsModeAfterTransfer() {
    if (csMode == 0 || csMode == 2) {
        setChipSelectAsserted(false);
    }
}

void FlashromSerprogAdapter::onUsbEvent(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    (void)arg;
    (void)eventBase;
    (void)eventData;

    if (eventId == ARDUINO_USB_CDC_CONNECTED_EVENT) {
        cdcConnected = true;
        resetSpiBusState();
        purgeInput();
        return;
    }

    if (eventId == ARDUINO_USB_CDC_DISCONNECTED_EVENT || eventId == ARDUINO_USB_CDC_RX_OVERFLOW_EVENT) {
        cdcConnected = false;
        resetSpiBusState();
        purgeInput();
    }
}

void FlashromSerprogAdapter::handleCommand(uint8_t command, IInput& input) {
    switch (command) {
        case CMD_NOP:
            writeAck();
            break;

        case CMD_Q_IFACE:
            writeAck();
            writeLe16(1);
            break;

        case CMD_Q_CMDMAP:
            writeAck();
            writeCommandMap();
            break;

        case CMD_Q_PGMNAME: {
            writeAck();
            const uint8_t name[16] = {
                'E', 'S', 'P', '3', '2', '-', 'B', 'P',
                '-', 'S', 'E', 'R', 'P', 'R', 'G', 0
            };
            hostSerial->write(name, sizeof(name));
            break;
        }

        case CMD_Q_SERBUF:
            writeAck();
            writeLe16(static_cast<uint16_t>(SERIAL_BUFFER_SIZE));
            break;

        case CMD_Q_BUSTYPE:
            writeAck();
            writeByte(BUS_SPI);
            break;

        case CMD_Q_WRNMAXLEN:
        case CMD_Q_RDNMAXLEN:
            writeAck();
            writeLe24(SERPROG_MAX_TRANSFER);
            break;

        case CMD_SYNCNOP:
            purgeInput();
            resetSpiBusState();
            writeNak();
            writeAck();
            break;

        case CMD_S_BUSTYPE: {
            uint8_t busType = 0;
            if (!readByte(input, busType)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            writeByte((busType & BUS_SPI) ? ACK : NAK);
            break;
        }

        case CMD_O_SPIOP:
            handleSpiOperation(input);
            break;

        case CMD_S_SPI_FREQ: {
            uint32_t requested = 0;
            if (!readLe32(input, requested)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            if (requested == 0) {
                requested = DEFAULT_SPI_FREQUENCY;
            }
            config.frequency = requested > MAX_SPI_FREQUENCY ? MAX_SPI_FREQUENCY : requested;
            writeAck();
            writeLe32(config.frequency);
            break;
        }

        case CMD_S_PIN_STATE:
        {
            uint8_t enabled = 0;
            if (!readByte(input, enabled)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            setPinDrivers(enabled != 0);
            writeAck();
            break;
        }

        case CMD_S_SPI_CS: {
            uint8_t csMode = 0;
            if (!readByte(input, csMode)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            writeByte(csMode == 0 ? ACK : NAK);
            break;
        }

        case CMD_S_SPI_MODE: {
            uint8_t requestedMode = 0;
            if (!readByte(input, requestedMode)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            if (requestedMode != 0) {
                writeNak();
                break;
            }
            writeAck();
            break;
        }

        case CMD_S_CS_MODE: {
            uint8_t requestedMode = 0;
            if (!readByte(input, requestedMode)) {
                purgeInput();
                resetSpiBusState();
                writeNak();
                break;
            }
            if (requestedMode > 2) {
                writeNak();
                break;
            }

            csMode = requestedMode;
            if (csMode == 1) {
                setChipSelectAsserted(true);
            } else if (csMode == 2) {
                setChipSelectAsserted(false);
            }
            writeAck();
            break;
        }

        default:
            writeNak();
            break;
    }
}

void FlashromSerprogAdapter::handleSpiOperation(IInput& input) {
    uint32_t writeLength = 0;
    uint32_t readLength = 0;

    if (!readLe24(input, writeLength) || !readLe24(input, readLength)) {
        purgeInput();
        resetSpiBusState();
        writeNak();
        return;
    }

    if (!pinsEnabled || writeLength > SERPROG_MAX_TRANSFER || readLength > SERPROG_MAX_TRANSFER) {
        purgeInput();
        resetSpiBusState();
        writeNak();
        return;
    }

    SPISettings settings(config.frequency, MSBFIRST, SPI_MODE0);
    spi.beginTransaction(settings);
    transactionActive = true;
    applyCsModeBeforeTransfer();

    for (uint32_t i = 0; i < writeLength; ++i) {
        if ((i & (TRANSFER_ABORT_CHECK_INTERVAL - 1)) == 0 && !cdcConnected) {
            resetSpiBusState();
            return;
        }

        uint8_t value = 0;
        if (!readByte(input, value)) {
            purgeInput();
            resetSpiBusState();
            writeNak();
            return;
        }

        spi.transfer(value);
    }

    writeAck();

    for (uint32_t i = 0; i < readLength; ++i) {
        if ((i & (TRANSFER_ABORT_CHECK_INTERVAL - 1)) == 0 && !cdcConnected) {
            resetSpiBusState();
            return;
        }

        if (!writeByte(spi.transfer(0x00))) {
            resetSpiBusState();
            return;
        }
    }

    applyCsModeAfterTransfer();
    spi.endTransaction();
    transactionActive = false;
}

bool FlashromSerprogAdapter::readByte(IInput& input, uint8_t& value, uint32_t timeoutMs) {
    unsigned long startMs = millis();
    while (hostSerial->available() <= 0) {
        if (!cdcConnected) {
            return false;
        }

        if (inputRequestedReset(input)) {
            setPinDrivers(false);
            ESP.restart();
        }

        if (timeoutMs != 0 && millis() - startMs >= timeoutMs) {
            return false;
        }
    }
    value = static_cast<uint8_t>(hostSerial->read());
    return true;
}

bool FlashromSerprogAdapter::readLe24(IInput& input, uint32_t& value) {
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    if (!readByte(input, b0, READ_TIMEOUT_MS) ||
        !readByte(input, b1, READ_TIMEOUT_MS) ||
        !readByte(input, b2, READ_TIMEOUT_MS)) {
        return false;
    }

    value = static_cast<uint32_t>(b0);
    value |= static_cast<uint32_t>(b1) << 8;
    value |= static_cast<uint32_t>(b2) << 16;
    return true;
}

bool FlashromSerprogAdapter::readLe32(IInput& input, uint32_t& value) {
    uint8_t b3 = 0;
    if (!readLe24(input, value) || !readByte(input, b3, READ_TIMEOUT_MS)) {
        return false;
    }

    value |= static_cast<uint32_t>(b3) << 24;
    return true;
}

bool FlashromSerprogAdapter::writeByte(uint8_t value) {
    if (!cdcConnected) {
        resetSpiBusState();
        return false;
    }

    unsigned long startMs = millis();
    while (hostSerial->availableForWrite() <= 0) {
        if (!cdcConnected) {
            resetSpiBusState();
            return false;
        }

        if (millis() - startMs >= TX_TIMEOUT_MS) {
            resetSpiBusState();
            return false;
        }

        yield();
    }

    if (hostSerial->write(value) != 1) {
        resetSpiBusState();
        return false;
    }

    return true;
}

bool FlashromSerprogAdapter::writeLe16(uint16_t value) {
    return writeByte(value & 0xFF) &&
           writeByte((value >> 8) & 0xFF);
}

bool FlashromSerprogAdapter::writeLe24(uint32_t value) {
    return writeByte(value & 0xFF) &&
           writeByte((value >> 8) & 0xFF) &&
           writeByte((value >> 16) & 0xFF);
}

bool FlashromSerprogAdapter::writeLe32(uint32_t value) {
    return writeLe24(value) &&
           writeByte((value >> 24) & 0xFF);
}

void FlashromSerprogAdapter::writeAck() {
    writeByte(ACK);
}

void FlashromSerprogAdapter::writeNak() {
    writeByte(NAK);
}

void FlashromSerprogAdapter::writeCommandMap() {
    uint8_t map[32] = {};
    const uint8_t supported[] = {
        CMD_NOP,
        CMD_Q_IFACE,
        CMD_Q_CMDMAP,
        CMD_Q_PGMNAME,
        CMD_Q_SERBUF,
        CMD_Q_BUSTYPE,
        CMD_Q_WRNMAXLEN,
        CMD_SYNCNOP,
        CMD_Q_RDNMAXLEN,
        CMD_S_BUSTYPE,
        CMD_O_SPIOP,
        CMD_S_SPI_FREQ,
        CMD_S_PIN_STATE,
        CMD_S_SPI_CS,
        CMD_S_SPI_MODE,
        CMD_S_CS_MODE,
    };

    for (uint8_t command : supported) {
        map[command / 8] |= 1 << (command % 8);
    }
    hostSerial->write(map, sizeof(map));
}

bool FlashromSerprogAdapter::inputRequestedReset(IInput& input) {
    return input.readChar() != KEY_NONE;
}
