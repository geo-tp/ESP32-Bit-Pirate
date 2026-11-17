#include <deque>
#include <initializer_list>
#include <string>

#include <unity.h>

#include "../Views/FakeTerminalView.h"
#include "Data/InputKeys.h"
#include "Managers/CommandHistoryManager.h"
#include "Managers/CommandLineManager.h"
#include "States/GlobalState.h"

namespace command_line_manager_tests {

class RecordingInput final : public IInput {
public:
    std::deque<char> chars;
    size_t readCharCalls = 0;
    size_t waitPressCalls = 0;

    char handler() override {
        return popOrEnter();
    }

    char readChar() override {
        ++readCharCalls;
        return popOrEnter();
    }

    void waitPress(uint32_t = 0) override {
        ++waitPressCalls;
    }

    void queue(std::initializer_list<char> values) {
        chars.insert(chars.end(), values.begin(), values.end());
    }

    void queue(const std::string& values) {
        chars.insert(chars.end(), values.begin(), values.end());
    }

private:
    char popOrEnter() {
        if (chars.empty()) return '\n';
        const char value = chars.front();
        chars.pop_front();
        return value;
    }
};

struct CommandLineFixture {
    FakeTerminalView view;
    RecordingInput terminalInput;
    RecordingInput deviceInput;
    CommandHistoryManager history;
    CommandLineManager manager{
        view,
        terminalInput,
        deviceInput,
        history
    };
};

void queueAnsi(RecordingInput& input, char code) {
    input.queue({'\x1B', '[', code});
}

void test_simple_input_followed_by_enter_returns_and_stores_command() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue({KEY_NONE, 's', 'c', 'a', 'n', '\r'});
    fixture.manager.waitPress();

    const std::string command = fixture.manager.readCommand("SPI");

    TEST_ASSERT_EQUAL_STRING("scan", command.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.history.size());
    TEST_ASSERT_EQUAL_STRING("scan", fixture.history.up().c_str());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.terminalInput.waitPressCalls);
    TEST_ASSERT_EQUAL_UINT32(6, fixture.deviceInput.readCharCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.view.printlnCalls.size());
}

void test_empty_command_returns_empty_and_is_not_stored() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue({'\n'});

    const std::string command = fixture.manager.readCommand("HIZ");

    TEST_ASSERT_TRUE(command.empty());
    TEST_ASSERT_EQUAL_UINT32(0, fixture.history.size());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.view.printlnCalls.size());
}

void test_backspace_handles_both_codes_and_start_of_line() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue(
        {'\b', 'a', 'b', '\b', 'c', static_cast<char>(127), 'd', '\n'}
    );

    const std::string command = fixture.manager.readCommand("DIO");

    TEST_ASSERT_EQUAL_STRING("ad", command.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("\rDIO> a \033[K"));
}

void test_cursor_insertion_preserves_tail() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue({'a', 'c'});
    queueAnsi(fixture.terminalInput, 'D');
    fixture.terminalInput.queue({'b', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "abc",
        fixture.manager.readCommand("UART").c_str()
    );
}

void test_left_and_right_arrows_obey_cursor_boundaries() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue({'a', 'b'});
    queueAnsi(fixture.terminalInput, 'D');
    queueAnsi(fixture.terminalInput, 'D');
    queueAnsi(fixture.terminalInput, 'D');
    queueAnsi(fixture.terminalInput, 'C');
    queueAnsi(fixture.terminalInput, 'C');
    queueAnsi(fixture.terminalInput, 'C');
    queueAnsi(fixture.terminalInput, 'D');
    fixture.terminalInput.queue({'X', '\n'});

    const std::string command = fixture.manager.readCommand("I2C");

    TEST_ASSERT_EQUAL_STRING("aXb", command.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("\x1B[C"));
    TEST_ASSERT_TRUE(fixture.view.contains("\x1B[D"));
}

void test_history_up_and_down_restore_and_clear_commands() {
    CommandLineFixture fixture;
    fixture.history.add("first");
    fixture.history.add("second");

    queueAnsi(fixture.terminalInput, 'A');
    queueAnsi(fixture.terminalInput, 'A');
    queueAnsi(fixture.terminalInput, 'B');
    fixture.terminalInput.queue({'\n'});

    TEST_ASSERT_EQUAL_STRING(
        "second",
        fixture.manager.readCommand("HIZ").c_str()
    );

    queueAnsi(fixture.terminalInput, 'A');
    queueAnsi(fixture.terminalInput, 'B');
    fixture.terminalInput.queue({'\n'});

    TEST_ASSERT_TRUE(fixture.manager.readCommand("HIZ").empty());
}

void test_tab_autocompletes_from_history_then_dictionary() {
    CommandLineFixture historyFixture;
    historyFixture.history.add("scan saved");
    historyFixture.terminalInput.queue({'s', 'c', 'a', '\t', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "scan saved",
        historyFixture.manager.readCommand("SPI").c_str()
    );

    CommandLineFixture dictionaryFixture;
    dictionaryFixture.terminalInput.queue({'h', 'e', '\t', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "help",
        dictionaryFixture.manager.readCommand("HIZ").c_str()
    );

    CommandLineFixture noMatchFixture;
    noMatchFixture.terminalInput.queue({'z', 'z', 'z', '\t', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "zzz",
        noMatchFixture.manager.readCommand("HIZ").c_str()
    );
}

void test_command_length_is_limited_to_512_characters() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue(std::string(520, 'x'));
    fixture.terminalInput.queue({'\n'});

    const std::string command = fixture.manager.readCommand("HIZ");

    TEST_ASSERT_EQUAL_UINT32(512, command.size());
    TEST_ASSERT_EQUAL_UINT32(512, fixture.view.printCalls.size());
}

void test_tab_completes_wifi_configuration_root_from_dictionary() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue(std::string("conf\t\n"));
    TEST_ASSERT_EQUAL_STRING(
        "config",
        fixture.manager.readCommand("WIFI").c_str()
    );
}

void test_complete_ansi_sequences_handle_all_four_arrows() {
    CommandLineFixture fixture;
    fixture.history.add("abc");
    queueAnsi(fixture.terminalInput, 'A');
    queueAnsi(fixture.terminalInput, 'D');
    queueAnsi(fixture.terminalInput, 'C');
    queueAnsi(fixture.terminalInput, 'B');
    fixture.terminalInput.queue({'z', '\n'});

    const std::string command = fixture.manager.readCommand("HIZ");

    TEST_ASSERT_EQUAL_STRING("z", command.c_str());
    TEST_ASSERT_TRUE(fixture.view.contains("\rHIZ> abc\033[K"));
}

void test_cardputer_sequences_keep_standalone_behavior() {
    CommandLineFixture fixture;
    GlobalState::getInstance().setTerminalMode(TerminalTypeEnum::Standalone);
    fixture.history.add("previous");
    fixture.terminalInput.queue(
        {CARDPUTER_SPECIAL_ARROW_UP, CARDPUTER_SPECIAL_ARROW_DOWN, '\t', '\n'}
    );

    const std::string command = fixture.manager.readCommand("SPI");

    TEST_ASSERT_EQUAL_STRING("previous", command.c_str());
    TEST_ASSERT_TRUE(
        fixture.view.contains(std::string(1, CARDPUTER_SPECIAL_ARROW_UP))
    );
    TEST_ASSERT_TRUE(
        fixture.view.contains(std::string(1, CARDPUTER_SPECIAL_ARROW_DOWN))
    );
    TEST_ASSERT_TRUE(fixture.view.contains("\rSPI> previous\033[K"));
}

void test_incomplete_and_unknown_escape_sequences_are_consumed_as_before() {
    CommandLineFixture incompleteFixture;
    incompleteFixture.terminalInput.queue({'\x1B', 'X', 'a', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "a",
        incompleteFixture.manager.readCommand("HIZ").c_str()
    );

    CommandLineFixture unknownFixture;
    unknownFixture.terminalInput.queue({'\x1B', '[', 'Z', 'b', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "b",
        unknownFixture.manager.readCommand("HIZ").c_str()
    );

    CommandLineFixture truncatedFixture;
    truncatedFixture.terminalInput.queue({'\x1B', '[', '\n'});

    TEST_ASSERT_TRUE(truncatedFixture.manager.readCommand("HIZ").empty());
}

void test_terminal_transcript_matches_legacy_action_dispatcher_behavior() {
    CommandLineFixture fixture;
    fixture.terminalInput.queue({'a', 'c'});
    queueAnsi(fixture.terminalInput, 'D');
    fixture.terminalInput.queue({'b', '\n'});

    TEST_ASSERT_EQUAL_STRING(
        "abc",
        fixture.manager.readCommand("SPI").c_str()
    );
    TEST_ASSERT_EQUAL_STRING(
        "\rSPI> a\033[K"
        "\rSPI> ac\033[K"
        "\x1B[D"
        "\rSPI> abc\033[K"
        "\x1B[D"
        "\n",
        fixture.view.output.c_str()
    );
}

}  // namespace command_line_manager_tests

void runCommandLineManagerTests() {
    using namespace command_line_manager_tests;
    RUN_TEST(test_simple_input_followed_by_enter_returns_and_stores_command);
    RUN_TEST(test_empty_command_returns_empty_and_is_not_stored);
    RUN_TEST(test_backspace_handles_both_codes_and_start_of_line);
    RUN_TEST(test_cursor_insertion_preserves_tail);
    RUN_TEST(test_left_and_right_arrows_obey_cursor_boundaries);
    RUN_TEST(test_history_up_and_down_restore_and_clear_commands);
    RUN_TEST(test_tab_autocompletes_from_history_then_dictionary);
    RUN_TEST(test_tab_completes_wifi_configuration_root_from_dictionary);
    RUN_TEST(test_command_length_is_limited_to_512_characters);
    RUN_TEST(test_complete_ansi_sequences_handle_all_four_arrows);
    RUN_TEST(test_cardputer_sequences_keep_standalone_behavior);
    RUN_TEST(test_incomplete_and_unknown_escape_sequences_are_consumed_as_before);
    RUN_TEST(test_terminal_transcript_matches_legacy_action_dispatcher_behavior);
}
