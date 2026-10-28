#include <unity.h>

#include <string>

#include "Enums/InfraredProtocolEnum.h"
#include "States/GlobalState.h"
#include "Transformers/ProfileTransformer.h"

namespace profile_transformer_tests {

bool containsText(const std::string& value, const std::string& text) {
    return value.find(text) != std::string::npos;
}

void test_to_profile_text_exports_representative_state_values() {
    auto& state = GlobalState::getInstance();
    state.setSpiCSPin(9);
    state.setI2cFrequency(400000);
    state.setUartBaudRate(115200);
    state.setUartInverted(true);
    state.setInfraredProtocol(InfraredProtocolEnum::_RC5);
    state.setLedBrightness(64);
    state.setI2sPercentLevel(75);
    state.setCanKbps(500);
    state.setSubGhzFrequency(315.25f);
    state.setEthernetIrqPin(26);

    const std::string profile = ProfileTransformer::toProfileText();

    TEST_ASSERT_TRUE(containsText(profile, "# ESP32 Bit Pirate profile"));
    TEST_ASSERT_TRUE(containsText(profile, "version=1.8"));
    TEST_ASSERT_TRUE(containsText(profile, "spi.cs=9"));
    TEST_ASSERT_TRUE(containsText(profile, "i2c.freq=400000"));
    TEST_ASSERT_TRUE(containsText(profile, "uart.baud=115200"));
    TEST_ASSERT_TRUE(containsText(profile, "uart.inverted=1"));
    TEST_ASSERT_TRUE(containsText(profile, "ir.protocol=rc5"));
    TEST_ASSERT_TRUE(containsText(profile, "led.brightness=64"));
    TEST_ASSERT_TRUE(containsText(profile, "i2s.level=75"));
    TEST_ASSERT_TRUE(containsText(profile, "can.kbps=500"));
    TEST_ASSERT_TRUE(containsText(profile, "subghz.freq=315.25"));
    TEST_ASSERT_TRUE(containsText(profile, "eth.irq=26"));
}

void test_from_profile_text_updates_supported_groups() {
    auto& state = GlobalState::getInstance();
    std::string error;

    const std::string profile =
        "# comments and blank lines are ignored\n"
        "\n"
        "spi.cs=10\n"
        "spi.clk=11\n"
        "spi.miso=12\n"
        "spi.mosi=13\n"
        "spi.freq=8000000\n"
        "i2c.sda=20\n"
        "i2c.scl=21\n"
        "i2c.freq=400000\n"
        "uart.baud=115200\n"
        "uart.rx=22\n"
        "uart.tx=23\n"
        "uart.databits=7\n"
        "uart.parity=Even\n"
        "uart.stopbits=2\n"
        "uart.inverted=1\n"
        "ir.tx=24\n"
        "ir.rx=25\n"
        "ir.protocol=rc5\n"
        "led.data=26\n"
        "led.clock=27\n"
        "led.len=128\n"
        "led.proto=apa102\n"
        "led.brightness=42\n"
        "hduart.baud=57600\n"
        "hduart.pin=28\n"
        "hduart.databits=9\n"
        "hduart.parity=Odd\n"
        "hduart.stopbits=2\n"
        "hduart.inverted=1\n"
        "i2s.bclk=29\n"
        "i2s.lrck=30\n"
        "i2s.data=31\n"
        "i2s.rate=48000\n"
        "i2s.bits=24\n"
        "i2s.level=80\n"
        "can.cs=32\n"
        "can.sck=33\n"
        "can.si=34\n"
        "can.so=35\n"
        "can.kbps=1000\n"
        "subghz.sck=36\n"
        "subghz.miso=37\n"
        "subghz.mosi=38\n"
        "subghz.cs=39\n"
        "subghz.gdo=40\n"
        "subghz.freq=915.5\n"
        "rf24.csn=41\n"
        "rf24.ce=42\n"
        "rf24.sck=43\n"
        "rf24.miso=44\n"
        "rf24.mosi=45\n"
        "rfid.sda=46\n"
        "rfid.scl=47\n"
        "sd.cs=48\n"
        "sd.clk=49\n"
        "sd.miso=50\n"
        "sd.mosi=51\n"
        "sd.freq=16000000\n"
        "eth.cs=52\n"
        "eth.clk=53\n"
        "eth.miso=54\n"
        "eth.mosi=55\n"
        "eth.irq=56\n"
        "eth.freq=25000000\n"
        "fm.sda=57\n"
        "fm.scl=58\n"
        "fm.rst=59\n";

    TEST_ASSERT_TRUE(ProfileTransformer::fromProfileText(profile, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());

    TEST_ASSERT_EQUAL_UINT8(10, state.getSpiCSPin());
    TEST_ASSERT_EQUAL_UINT8(11, state.getSpiCLKPin());
    TEST_ASSERT_EQUAL_UINT8(12, state.getSpiMISOPin());
    TEST_ASSERT_EQUAL_UINT8(13, state.getSpiMOSIPin());
    TEST_ASSERT_EQUAL_UINT32(8000000, state.getSpiFrequency());
    TEST_ASSERT_EQUAL_UINT8(20, state.getI2cSdaPin());
    TEST_ASSERT_EQUAL_UINT8(21, state.getI2cSclPin());
    TEST_ASSERT_EQUAL_UINT32(400000, state.getI2cFrequency());
    TEST_ASSERT_EQUAL_UINT32(115200, state.getUartBaudRate());
    TEST_ASSERT_EQUAL_UINT8(22, state.getUartRxPin());
    TEST_ASSERT_EQUAL_UINT8(23, state.getUartTxPin());
    TEST_ASSERT_EQUAL_UINT8(7, state.getUartDataBits());
    TEST_ASSERT_EQUAL_STRING("Even", state.getUartParity().c_str());
    TEST_ASSERT_EQUAL_UINT8(2, state.getUartStopBits());
    TEST_ASSERT_TRUE(state.isUartInverted());
    TEST_ASSERT_EQUAL_UINT8(24, state.getInfraredTxPin());
    TEST_ASSERT_EQUAL_UINT8(25, state.getInfraredRxPin());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(InfraredProtocolEnum::_RC5),
                          static_cast<int>(state.getInfraredProtocol()));
    TEST_ASSERT_EQUAL_UINT8(26, state.getLedDataPin());
    TEST_ASSERT_EQUAL_UINT8(27, state.getLedClockPin());
    TEST_ASSERT_EQUAL_UINT16(128, state.getLedLength());
    TEST_ASSERT_EQUAL_STRING("apa102", state.getLedProtocol().c_str());
    TEST_ASSERT_EQUAL_UINT8(42, state.getLedBrightness());
    TEST_ASSERT_EQUAL_UINT32(57600, state.getHdUartBaudRate());
    TEST_ASSERT_EQUAL_UINT8(28, state.getHdUartPin());
    TEST_ASSERT_EQUAL_UINT8(9, state.getHdUartDataBits());
    TEST_ASSERT_EQUAL_STRING("Odd", state.getHdUartParity().c_str());
    TEST_ASSERT_EQUAL_UINT8(2, state.getHdUartStopBits());
    TEST_ASSERT_TRUE(state.isHdUartInverted());
    TEST_ASSERT_EQUAL_UINT8(29, state.getI2sBclkPin());
    TEST_ASSERT_EQUAL_UINT8(30, state.getI2sLrckPin());
    TEST_ASSERT_EQUAL_UINT8(31, state.getI2sDataPin());
    TEST_ASSERT_EQUAL_UINT32(48000, state.getI2sSampleRate());
    TEST_ASSERT_EQUAL_UINT8(24, state.getI2sBitsPerSample());
    TEST_ASSERT_EQUAL_UINT8(80, state.getI2sPercentLevel());
    TEST_ASSERT_EQUAL_UINT8(32, state.getCanCspin());
    TEST_ASSERT_EQUAL_UINT8(33, state.getCanSckPin());
    TEST_ASSERT_EQUAL_UINT8(34, state.getCanSiPin());
    TEST_ASSERT_EQUAL_UINT8(35, state.getCanSoPin());
    TEST_ASSERT_EQUAL_UINT32(1000, state.getCanKbps());
    TEST_ASSERT_EQUAL_UINT8(36, state.getSubGhzSckPin());
    TEST_ASSERT_EQUAL_UINT8(37, state.getSubGhzMisoPin());
    TEST_ASSERT_EQUAL_UINT8(38, state.getSubGhzMosiPin());
    TEST_ASSERT_EQUAL_UINT8(39, state.getSubGhzCsPin());
    TEST_ASSERT_EQUAL_UINT8(40, state.getSubGhzGdoPin());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 915.5f, state.getSubGhzFrequency());
    TEST_ASSERT_EQUAL_UINT8(41, state.getRf24CsnPin());
    TEST_ASSERT_EQUAL_UINT8(42, state.getRf24CePin());
    TEST_ASSERT_EQUAL_UINT8(43, state.getRf24SckPin());
    TEST_ASSERT_EQUAL_UINT8(44, state.getRf24MisoPin());
    TEST_ASSERT_EQUAL_UINT8(45, state.getRf24MosiPin());
    TEST_ASSERT_EQUAL_UINT8(46, state.getRfidSdaPin());
    TEST_ASSERT_EQUAL_UINT8(47, state.getRfidSclPin());
    TEST_ASSERT_EQUAL_UINT8(48, state.getSdCardCsPin());
    TEST_ASSERT_EQUAL_UINT8(49, state.getSdCardClkPin());
    TEST_ASSERT_EQUAL_UINT8(50, state.getSdCardMisoPin());
    TEST_ASSERT_EQUAL_UINT8(51, state.getSdCardMosiPin());
    TEST_ASSERT_EQUAL_UINT32(16000000, state.getSdCardFrequency());
    TEST_ASSERT_EQUAL_UINT8(52, state.getEthernetCsPin());
    TEST_ASSERT_EQUAL_UINT8(53, state.getEthernetSckPin());
    TEST_ASSERT_EQUAL_UINT8(54, state.getEthernetMisoPin());
    TEST_ASSERT_EQUAL_UINT8(55, state.getEthernetMosiPin());
    TEST_ASSERT_EQUAL_UINT8(56, state.getEthernetIrqPin());
    TEST_ASSERT_EQUAL_UINT32(25000000, state.getEthernetFrequency());
    TEST_ASSERT_EQUAL_UINT8(57, state.getTwoWireIoPin());
    TEST_ASSERT_EQUAL_UINT8(58, state.getTwoWireClkPin());
    TEST_ASSERT_EQUAL_UINT8(59, state.getTwoWireRstPin());
}

void test_from_profile_text_cell_fields_update_uart_profile() {
    auto& state = GlobalState::getInstance();
    std::string error;

    TEST_ASSERT_TRUE(ProfileTransformer::fromProfileText(
        "cell.tx=60\n"
        "cell.rx=61\n"
        "cell.baud=38400\n",
        error));

    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT8(60, state.getUartTxPin());
    TEST_ASSERT_EQUAL_UINT8(61, state.getUartRxPin());
    TEST_ASSERT_EQUAL_UINT32(38400, state.getUartBaudRate());
}

void test_from_profile_text_partial_profile_does_not_reuse_previous_value_for_missing_keys() {
    auto& state = GlobalState::getInstance();
    std::string error;

    TEST_ASSERT_TRUE(ProfileTransformer::fromProfileText("spi.cs=7\n", error));

    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT8(7, state.getSpiCSPin());
    TEST_ASSERT_EQUAL_UINT8(40, state.getSpiCLKPin());
    TEST_ASSERT_EQUAL_UINT8(39, state.getSpiMISOPin());
    TEST_ASSERT_EQUAL_UINT8(14, state.getSpiMOSIPin());
    TEST_ASSERT_EQUAL_UINT32(20000000, state.getSpiFrequency());
}

void test_from_profile_text_rejects_invalid_values_without_overwriting_target() {
    auto& state = GlobalState::getInstance();
    std::string error;

    TEST_ASSERT_FALSE(ProfileTransformer::fromProfileText("eth.cs=999\n", error));
    TEST_ASSERT_EQUAL_STRING("Invalid eth.cs", error.c_str());
    TEST_ASSERT_EQUAL_UINT8(5, state.getEthernetCsPin());

    error.clear();
    TEST_ASSERT_FALSE(ProfileTransformer::fromProfileText("i2s.level=nope\n", error));
    TEST_ASSERT_EQUAL_STRING("Invalid i2s.level", error.c_str());
    TEST_ASSERT_EQUAL_UINT8(100, state.getI2sPercentLevel());
}

}  // namespace profile_transformer_tests

void runProfileTransformerTests() {
    using namespace profile_transformer_tests;
    RUN_TEST(test_to_profile_text_exports_representative_state_values);
    RUN_TEST(test_from_profile_text_updates_supported_groups);
    RUN_TEST(test_from_profile_text_cell_fields_update_uart_profile);
    RUN_TEST(test_from_profile_text_partial_profile_does_not_reuse_previous_value_for_missing_keys);
    RUN_TEST(test_from_profile_text_rejects_invalid_values_without_overwriting_target);
}
