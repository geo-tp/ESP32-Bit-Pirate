#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"

// Exclusive one-shot USB CDC adapter for flashrom's serprog protocol.
// Implements the SPI subset used for external SPI flash read/write.

struct FlashromSerprogConfig {
    uint8_t csPin;
    uint8_t sckPin;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint32_t frequency;
    int8_t wpPin = -1;
    int8_t holdPin = -1;
};

class FlashromSerprogAdapter {
public:
    static void run(const FlashromSerprogConfig& config, IInput& input, IHostSerial& hostSerial);

private:
    static inline FlashromSerprogConfig config = {0, 0, 0, 0, 8000000};
    static inline IHostSerial* hostSerial = nullptr;
    static inline SPIClass spi{HSPI};
    static inline bool pinsEnabled = false;
    static inline bool transactionActive = false;
    static inline bool cdcConnected = false;
    static inline uint8_t csMode = 0;

    static void initializeSpi();
    static void setPinDrivers(bool enabled);
    static void setChipSelectAsserted(bool asserted);
    static void resetSpiBusState();
    static void purgeInput();
    static void applyCsModeBeforeTransfer();
    static void applyCsModeAfterTransfer();
    static void onUsbEvent(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
    static void handleCommand(uint8_t command, IInput& input);
    static void handleSpiOperation(IInput& input);
    static bool readByte(IInput& input, uint8_t& value, uint32_t timeoutMs = 1000);
    static bool readLe24(IInput& input, uint32_t& value);
    static bool readLe32(IInput& input, uint32_t& value);
    static bool writeByte(uint8_t value);
    static bool writeLe16(uint16_t value);
    static bool writeLe24(uint32_t value);
    static bool writeLe32(uint32_t value);
    static void writeAck();
    static void writeNak();
    static void writeCommandMap();
    static bool inputRequestedReset(IInput& input);
};
