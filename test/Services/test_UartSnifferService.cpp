#include <unity.h>

#include "Services/UartSnifferService.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeUartService.h"
#include "../Services/FakeUtilityService.h"
#include "../Views/FakeTerminalView.h"

namespace uart_sniffer_service_tests {

struct UartSnifferServiceFixture {
    FakeTerminalView view;
    FakeInput input;
    FakeUtilityService utility;
    FakeUartService firstPort;
    FakeUartService secondPort;
    UartSnifferService service{firstPort, nullptr, secondPort, nullptr};
};

void test_sniff_text_uses_injected_ports_and_prints_both_lines() {
    UartSnifferServiceFixture fixture;
    fixture.firstPort.rxAfterSetRxFIFOFull = "A";
    fixture.secondPort.rxAfterSetRxFIFOFull = "B";
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar(KEY_NONE);
    fixture.input.queueReadChar('\n');

    fixture.service.sniffText(fixture.view, fixture.input, fixture.utility,
                              57600, 0x11223344, true, 8, 9);

    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(57600, fixture.firstPort.configurations[0].baud);
    TEST_ASSERT_EQUAL_HEX32(0x11223344, fixture.firstPort.configurations[0].config);
    TEST_ASSERT_EQUAL_UINT8(8, fixture.firstPort.configurations[0].rx);
    TEST_ASSERT_EQUAL_INT8(-1, fixture.firstPort.configurations[0].tx);
    TEST_ASSERT_TRUE(fixture.firstPort.configurations[0].inverted);
    TEST_ASSERT_TRUE(fixture.firstPort.configurations[0].noAllocation);
    TEST_ASSERT_NULL(fixture.firstPort.configurations[0].serial);

    TEST_ASSERT_EQUAL_UINT8(9, fixture.secondPort.configurations[0].rx);
    TEST_ASSERT_EQUAL_INT8(-1, fixture.secondPort.configurations[0].tx);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.setRxFIFOFullCalls);
    TEST_ASSERT_EQUAL_UINT8(1, fixture.firstPort.lastRxFIFOFull);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.setRxFIFOFullCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.endCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("[RX] A"));
    TEST_ASSERT_TRUE(fixture.view.contains("[TX] B"));
    TEST_ASSERT_TRUE(fixture.view.contains("UART Sniff: Stopped by user."));
}

void test_sniff_raw_configures_and_closes_ports_even_without_data() {
    UartSnifferServiceFixture fixture;
    fixture.input.queueReadChar('\n');

    fixture.service.sniffRaw(fixture.view, fixture.input, fixture.utility,
                             115200, 0x55667788, false, 10, 11);

    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.configurations.size());
    TEST_ASSERT_EQUAL_UINT32(115200, fixture.firstPort.configurations[0].baud);
    TEST_ASSERT_EQUAL_HEX32(0x55667788, fixture.secondPort.configurations[0].config);
    TEST_ASSERT_EQUAL_UINT8(10, fixture.firstPort.configurations[0].rx);
    TEST_ASSERT_EQUAL_UINT8(11, fixture.secondPort.configurations[0].rx);
    TEST_ASSERT_FALSE(fixture.firstPort.configurations[0].inverted);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.flushCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.flushCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.firstPort.endCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.secondPort.endCalls);
    TEST_ASSERT_TRUE(fixture.view.contains("UART Sniff: Stopped by user."));
}

}  // namespace uart_sniffer_service_tests

void runUartSnifferServiceTests() {
    using namespace uart_sniffer_service_tests;
    RUN_TEST(test_sniff_text_uses_injected_ports_and_prints_both_lines);
    RUN_TEST(test_sniff_raw_configures_and_closes_ports_even_without_data);
}
