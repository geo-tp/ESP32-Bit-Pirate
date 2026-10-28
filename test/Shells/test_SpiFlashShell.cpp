#include <unity.h>
#include "Shells/SpiFlashShell.h"
#include "../Inputs/FakeInput.h"
#include "../Services/FakeSpiService.h"
#include "../Views/FakeTerminalView.h"

namespace spi_flash_shell_tests {
struct Fixture {
    FakeTerminalView view;
    FakeInput input;
    FakeSpiService spi;
    ArgTransformer transformer;
    UserInputManager userInput{view, input, transformer};
    BinaryAnalyzer analyzer{view, input};
    SpiFlashShell shell{spi, view, input, transformer, userInput, analyzer};
};

void test_long_strings_stream_without_splitting_lines_or_losing_eof_tail() {
    Fixture fixture;
    fixture.spi.flashId = {0x1c, 0x20, 0x10}; // 64 KiB.
    fixture.spi.flashReader = [](uint32_t address, uint8_t* buffer, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            const uint32_t pos = address + i;
            buffer[i] = pos < 1200 ? 'A' : pos >= 65536 - 600 ? 'Z' : pos == 1201 ? 'x' : 0;
        }
    };
    fixture.input.queueLine("4"); // Extract strings.
    fixture.input.queueLine("10");
    fixture.input.queueLine("10"); // Exit.
    for (uint32_t i = 0; i < 65536; ++i) fixture.input.queueReadChar(KEY_NONE);
    fixture.shell.run();
    TEST_ASSERT_TRUE(fixture.view.contains("0x000000: " + std::string(1200, 'A') + "\n"));
    TEST_ASSERT_TRUE(fixture.view.contains("0x00FDA8: " + std::string(600, 'Z') + "\n"));
    TEST_ASSERT_FALSE(fixture.view.contains("0x0004B1:")); // Below minimum length.
    for (const auto& part : fixture.view.printCalls) TEST_ASSERT_TRUE(part.size() <= 512);
}

void test_oversize_erase_block_explains_ram_limit_and_programs_only_after_confirmation() {
    for (bool confirm : {false, true}) {
        Fixture fixture;
        fixture.spi.flashId = {0x20, 0x20, 0x18}; // M25P128: 256 KiB erase blocks.
        fixture.input.queueLine("6"); // Write bytes.
        fixture.input.queueLine("0"); // Address.
        fixture.input.queueLine("y"); // ASCII.
        fixture.input.queueLine("test");
        fixture.input.queueLine(confirm ? "y" : "n");
        fixture.input.queueLine("10");
        fixture.shell.run();
        TEST_ASSERT_TRUE(fixture.view.contains("64 KiB RAM limit"));
        TEST_ASSERT_TRUE(fixture.view.contains("Programming without erase."));
        TEST_ASSERT_EQUAL_UINT32(0, fixture.spi.patchWriteCalls);
        TEST_ASSERT_EQUAL_UINT32(confirm ? 1 : 0, fixture.spi.pageWriteCalls);
    }
}
} // namespace spi_flash_shell_tests

void runSpiFlashShellTests() {
    using namespace spi_flash_shell_tests;
    RUN_TEST(test_long_strings_stream_without_splitting_lines_or_losing_eof_tail);
    RUN_TEST(test_oversize_erase_block_explains_ram_limit_and_programs_only_after_confirmation);
}
