#include <unity.h>

#include "Services/FakeI2cSnifferService.h"
#include "States/TestGlobalState.h"

// Production units exercised by the native test runner.
#include "../src/Transformers/ArgTransformer.cpp"
#include "../src/Transformers/TerminalCommandTransformer.cpp"
#include "../src/Transformers/InstructionTransformer.cpp"
#include "../src/Transformers/PinoutTransformer.cpp"
#include "../src/Transformers/AtTransformer.cpp"
#include "../src/Transformers/InfraredRemoteTransformer.cpp"
#include "../src/Transformers/Bpio2Transformer.cpp"
#include "../src/Transformers/LoRaTransformer.cpp"
#include "../src/Transformers/SubGhzTransformer.cpp"
#include "../src/Transformers/ProfileTransformer.cpp"
#include "../src/Transformers/JsonTransformer.cpp"
#include "../src/Transformers/WebRequestTransformer.cpp"
#include "../src/Analyzers/BinaryAnalyzer.cpp"
#include "../src/Analyzers/PinAnalyzer.cpp"
#include "../src/Analyzers/I2cFrequencyAnalyzer.cpp"
#include "../src/Analyzers/SubGhzAnalyzer.cpp"
#include "../src/Managers/AliasManager.cpp"
#include "../src/Managers/CommandHistoryManager.cpp"
#include "../src/Managers/CommandLineManager.cpp"
#include "../src/Managers/UserInputManager.cpp"
#include "../src/Services/UartSnifferService.cpp"
#include "../src/Selectors/HorizontalSelector.cpp"
#include "../src/Shells/HelpShell.cpp"
#include "../src/Shells/SpiFlashShell.cpp"
#include "../src/Shells/MouseShell.cpp"
#include "../src/Abstracts/ANetworkController.cpp"
#include "../src/Controllers/CanController.cpp"
#include "../src/Controllers/DioController.cpp"
#include "../src/Controllers/I2sController.cpp"
#include "../src/Controllers/JtagController.cpp"
#include "../src/Controllers/LedController.cpp"
#include "../src/Controllers/Rf24Controller.cpp"
#include "../src/Controllers/RfidController.cpp"
#include "../src/Controllers/SubGhzController.cpp"
#include "../src/Controllers/HdUartController.cpp"
#include "../src/Controllers/ExpanderController.cpp"
#include "../src/Controllers/SpiController.cpp"
#include "../src/Controllers/I2cController.cpp"
#include "../src/Controllers/OneWireController.cpp"
#include "../src/Controllers/TwoWireController.cpp"
#include "../src/Controllers/ThreeWireController.cpp"
#include "../src/Controllers/LoRaController.cpp"
#include "../src/Controllers/InfraredController.cpp"
#include "../src/Controllers/FmController.cpp"
#include "../src/Controllers/CellController.cpp"
#include "../src/Controllers/BluetoothController.cpp"
#include "../src/Controllers/WifiController.cpp"
#include "../src/Controllers/EthernetController.cpp"
#include "../src/Controllers/UsbS3Controller.cpp"
#include "../src/Controllers/UartController.cpp"
#include "../src/Controllers/UtilityController.cpp"

// Test suites.
#include "Transformers/test_ArgTransformer.cpp"
#include "Transformers/test_TerminalCommandTransformer.cpp"
#include "Transformers/test_InstructionTransformer.cpp"
#include "Transformers/test_PinoutTransformer.cpp"
#include "Transformers/test_AtTransformer.cpp"
#include "Transformers/test_InfraredRemoteTransformer.cpp"
#include "Transformers/test_Bpio2Transformer.cpp"
#include "Transformers/test_LoRaTransformer.cpp"
#include "Transformers/test_SubGhzTransformer.cpp"
#include "Transformers/test_ProfileTransformer.cpp"
#include "Transformers/test_JsonTransformer.cpp"
#include "Transformers/test_WebRequestTransformer.cpp"
#include "Enums/test_ModeEnum.cpp"
#include "Enums/test_TerminalTypeEnum.cpp"
#include "Enums/test_TerminalModeEnum.cpp"
#include "Enums/test_LedProtocolEnum.cpp"
#include "Enums/test_LedChipsetEnum.cpp"
#include "Enums/test_SubGhzProtocolEnum.cpp"
#include "Enums/test_InfraredProtocolEnum.cpp"
#include "Analyzers/test_BinaryAnalyzer.cpp"
#include "Analyzers/test_PinAnalyzer.cpp"
#include "Analyzers/test_I2cFrequencyAnalyzer.cpp"
#include "Analyzers/test_SubGhzAnalyzer.cpp"
#include "Selectors/test_HorizontalSelector.cpp"
#include "Managers/test_AliasManager.cpp"
#include "Managers/test_CommandHistoryManager.cpp"
#include "Managers/test_CommandLineManager.cpp"
#include "Managers/test_UserInputManager.cpp"
#include "Services/test_UartSnifferService.cpp"
#include "Abstracts/test_ANetworkController.cpp"
#include "Controllers/test_CanController.cpp"
#include "Controllers/test_DioController.cpp"
#include "Controllers/test_I2sController.cpp"
#include "Controllers/test_JtagController.cpp"
#include "Controllers/test_LedController.cpp"
#include "Controllers/test_Rf24Controller.cpp"
#include "Controllers/test_RfidController.cpp"
#include "Controllers/test_SubGhzController.cpp"
#include "Controllers/test_HdUartController.cpp"
#include "Controllers/test_ExpanderController.cpp"
#include "Controllers/test_SpiController.cpp"
#include "Shells/test_SpiFlashShell.cpp"
#include "Controllers/test_I2cController.cpp"
#include "Controllers/test_OneWireController.cpp"
#include "Controllers/test_TwoWireController.cpp"
#include "Controllers/test_ThreeWireController.cpp"
#include "Controllers/test_LoRaController.cpp"
#include "Controllers/test_InfraredController.cpp"
#include "Controllers/test_FmController.cpp"
#include "Controllers/test_CellController.cpp"
#include "Controllers/test_BluetoothController.cpp"
#include "Controllers/test_WifiController.cpp"
#include "Controllers/test_EthernetController.cpp"
#include "Controllers/test_UsbS3Controller.cpp"
#include "Controllers/test_UartController.cpp"
#include "Controllers/test_UtilityController.cpp"

void setUp() {
    test_state::resetGlobalState();
}

void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    runArgTransformerTests();
    runTerminalCommandTransformerTests();
    runInstructionTransformerTests();
    runPinoutTransformerTests();
    runAtTransformerTests();
    runInfraredRemoteTransformerTests();
    runBpio2TransformerTests();
    runLoRaTransformerTests();
    runSubGhzTransformerTests();
    runProfileTransformerTests();
    runJsonTransformerTests();
    runWebRequestTransformerTests();
    runModeEnumTests();
    runTerminalTypeEnumTests();
    runTerminalModeEnumTests();
    runLedProtocolEnumTests();
    runLedChipsetEnumTests();
    runSubGhzProtocolEnumTests();
    runInfraredProtocolEnumTests();
    runBinaryAnalyzerTests();
    runPinAnalyzerTests();
    runI2cFrequencyAnalyzerTests();
    runSubGhzAnalyzerTests();
    runHorizontalSelectorTests();
    runAliasManagerTests();
    runCommandHistoryManagerTests();
    runCommandLineManagerTests();
    runUserInputManagerTests();
    runUartSnifferServiceTests();
    runANetworkControllerTests();
    runCanControllerTests();
    runDioControllerTests();
    runI2sControllerTests();
    runJtagControllerTests();
    runLedControllerTests();
    runRf24ControllerTests();
    runRfidControllerTests();
    runSubGhzControllerTests();
    runHdUartControllerTests();
    runExpanderControllerTests();
    runSpiControllerTests();
    runSpiFlashShellTests();
    runI2cControllerTests();
    runOneWireControllerTests();
    runTwoWireControllerTests();
    runThreeWireControllerTests();
    runLoRaControllerTests();
    runInfraredControllerTests();
    runFmControllerTests();
    runCellControllerTests();
    runBluetoothControllerTests();
    runWifiControllerTests();
    runEthernetControllerTests();
    runUsbS3ControllerTests();
    runUartControllerTests();
    runUtilityControllerTests();
    return UNITY_END();
}
