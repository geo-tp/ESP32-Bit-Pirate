#pragma once

#include <array>
#include <vector>

#include "States/GlobalState.h"

namespace test_state {

inline void resetGlobalState() {
    auto& state = GlobalState::getInstance();

    state.setActiveApName(state.getApName());
    state.setTerminalIp("0.0.0.0");
    state.setTerminalMode(TerminalTypeEnum::SerialPort);
    state.setCurrentMode(ModeEnum::HIZ);

    state.setSpiCSPin(12);
    state.setSpiCLKPin(40);
    state.setSpiMISOPin(39);
    state.setSpiMOSIPin(14);
    state.setSpiWPPin(-1);
    state.setSpiHOLDPin(-1);
    state.setSpiFrequency(20000000);

    state.setOneWirePin(1);

    state.setTwoWireClkPin(1);
    state.setTwoWireIoPin(2);
    state.setTwoWireRstPin(3);

    state.setThreeWireCsPin(5);
    state.setThreeWireSkPin(18);
    state.setThreeWireDiPin(23);
    state.setThreeWireDoPin(19);
    state.setThreeWireOrg8(false);
    state.setThreeWireEepromModelIndex(0);

    state.setUartBaudRate(9600);
    state.setUartConfig(0x800001c);
    state.setUartInverted(false);
    state.setUartRxPin(1);
    state.setUartTxPin(2);
    state.setUartDataBits(8);
    state.setUartParity("None");
    state.setUartFlowControl(false);
    state.setUartStopBits(1);

    state.setHdUartBaudRate(9600);
    state.setHdUartConfig(0x800001c);
    state.setHdUartInverted(false);
    state.setHdUartPin(1);
    state.setHdUartDataBits(8);
    state.setHdUartParity("N");
    state.setHdUartFlowControl(false);
    state.setHdUartStopBits(1);

    state.setI2cSclPin(1);
    state.setI2cSdaPin(2);
    state.setI2cFrequency(100000);

    state.setInfraredTxPin(44);
    state.setInfraredRxPin(1);
    state.setInfraredProtocol(InfraredProtocolEnum::_NEC);

    state.setLedDataPin(1);
    state.setLedClockPin(2);
    state.setLedLength(1);
    state.setLedProtocol("ws2812");
    state.setLedBrightness(128);

    state.setI2sBclkPin(41);
    state.setI2sLrckPin(43);
    state.setI2sDataPin(42);
    state.setI2sSampleRate(44100);
    state.setI2sBitsPerSample(16);
    state.setI2sPercentLevel(100);

    state.setJtagScanPins({1, 3, 5, 7, 9});

    state.setCanCspin(1);
    state.setCanSckPin(0);
    state.setCanSiPin(2);
    state.setCanSoPin(3);
    state.setCanKbps(120);

    state.setEthernetCsPin(5);
    state.setEthernetRstPin(4);
    state.setEthernetSckPin(18);
    state.setEthernetMisoPin(19);
    state.setEthernetMosiPin(23);
    state.setEthernetIrqPin(39);
    state.setEthernetFrequency(20000000);
    state.setEthernetMac({0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42});

    state.setSubGhzSckPin(12);
    state.setSubGhzMisoPin(13);
    state.setSubGhzMosiPin(14);
    state.setSubGhzCsPin(15);
    state.setSubGhzGdoPin(27);
    state.setSubGhzFrequency(433.92f);

    state.setRf24CsnPin(1);
    state.setRf24CePin(3);
    state.setRf24SckPin(5);
    state.setRf24MisoPin(7);
    state.setRf24MosiPin(9);

    state.setLoRaSckPin(5);
    state.setLoRaMisoPin(6);
    state.setLoRaMosiPin(7);
    state.setLoRaCsPin(4);
    state.setLoRaRstPin(3);
    state.setLoRaBusyPin(2);
    state.setLoRaDio1Pin(1);
    state.setLoRaFrequency(868.0f);
    state.setLoRaTcxoVoltage(1.8f);
    state.setLoRaBandwidth(125);
    state.setLoRaSpreadingFactor(9);
    state.setLoRaCodingRate(7);
    state.setLoRaPower(14);
    state.setLoRaPreambleLength(8);
    state.setLoRaSyncWord(0x1424);
    state.setLoRaCrc(true);
    state.setLoRaInvertIq(false);

    state.setRfidSdaPin(1);
    state.setRfidSclPin(2);

    state.setSdCardCsPin(12);
    state.setSdCardClkPin(40);
    state.setSdCardMisoPin(39);
    state.setSdCardMosiPin(14);
    state.setSdCardFrequency(20000000);
}

}  // namespace test_state
