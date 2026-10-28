#pragma once

#include "Interfaces/ITerminalView.h"
#include "Data/FlashDatabase.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IShell.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Interfaces/ISpiService.h"
#include "Analyzers/BinaryAnalyzer.h"
#include "Models/TerminalCommand.h"
#include "States/GlobalState.h"

class SpiFlashShell : public IShell {
public:
    SpiFlashShell(
        ISpiService& spiService,
        ITerminalView& view,
        IInput& input,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        BinaryAnalyzer& binaryAnalyzer
    );

    void run() override;

private:
    inline static constexpr const char* actions[] = {
        " 🔍 Probe Flash",
        " 📊 Analyze Flash",
        " 🔎 Search string",
        " 📜 Extract strings",
        " 📖 Read bytes",
        " ✏️  Write bytes",
        " 🗃️  Dump ASCII",
        " 🗃️  Dump RAW",
        " 💣 Erase Flash",
        "🚪 Exit Shell"
    };
    inline static constexpr size_t actionCount = sizeof(actions) / sizeof(actions[0]);
    
    ISpiService& spiService;
    ITerminalView& terminalView;
    IInput& terminalInput;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    BinaryAnalyzer& binaryAnalyzer;
    GlobalState& state = GlobalState::getInstance();

    void cmdProbe();
    void cmdAnalyze();
    void cmdSearch();
    void cmdStrings();
    void cmdRead();
    void cmdWrite();
    void cmdErase();
    void cmdDump(bool raw = false);
    void readFlashInChunks(uint32_t address, uint32_t length);
    void readFlashInChunksRaw(uint32_t address, uint32_t length);
    uint32_t readFlashCapacity();
    bool checkFlashPresent(FlashOperation operation = FlashOperation::Read);
    bool checkFlashRange(uint32_t address, size_t length);
};
