#include "DependencyProvider.h"

DependencyProvider::DependencyProvider(ITerminalView &terminalView, IDeviceView &deviceView,
                                       IInput &terminalInput, IInput &deviceInput,
                                       LittleFsService &littleFsService)
    : terminalView(terminalView),
      deviceView(deviceView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      littleFsService(littleFsService),

      // Services
      sdService(),
      nvsService(),
      ledService(),
      uartService(),
      uartSnifferFirstPort(),
      uartSnifferSecondPort(),
      uartSnifferService(uartSnifferFirstPort, &Serial1, uartSnifferSecondPort, &Serial2),
      i2cService(),
      oneWireService(),
      twoWireService(),
      threeWireService(),
      infraredService(),
      spiService(),
      pinService(),
      bluetoothService(),
      wifiService(),
      wifiScannerService(),
      i2sService(),
      sshService(),
      jtagService(),
      canService(),
      systemService(),
      utilityService(),
      ethernetService(),
      httpService(),
      telnetService(),
      modbusService(),
      subGhzService(),
      rfidService(),
      rf24Service(),
      usbService(),
      cellService(),
      fmService(),
      loRaService(),
      meshtasticService(),

      // Transformers
      commandTransformer(),
      instructionTransformer(),
      argTransformer(),
      webRequestTransformer(),
      jsonTransformer(),
      infraredTransformer(),
      subGhzTransformer(),
      loRaTransformer(utilityService),
      profileTransformer(),
      atTransformer(),

      // Managers
      commandHistoryManager(),
      commandLineManager(terminalView, terminalInput, deviceInput, commandHistoryManager),
      binaryAnalyzer(terminalView, terminalInput),
      userInputManager(terminalView, terminalInput, argTransformer),
      subGhzAnalyzer(),
      pinAnalyzer(pinService, utilityService),
      aliasManager(),

      // Shells
      sdCardShell(sdService, terminalView, terminalInput, argTransformer, userInputManager),
      spiFlashShell(spiService, terminalView, terminalInput, argTransformer, userInputManager, binaryAnalyzer),
      spiEepromShell(spiService, terminalView, terminalInput, argTransformer, userInputManager, binaryAnalyzer),
      smartCardShell(twoWireService, terminalView, terminalInput, utilityService, argTransformer, userInputManager),
      universalRemoteShell(terminalView, terminalInput, utilityService, infraredService, argTransformer, userInputManager),
      ibuttonShell(terminalView, terminalInput, utilityService, userInputManager, argTransformer, oneWireService),
      i2cEepromShell(terminalView, terminalInput, i2cService, argTransformer, userInputManager, binaryAnalyzer),
      uartAtShell(terminalView, terminalInput, utilityService, userInputManager, argTransformer, uartService),
      threeWireEepromShell(terminalView, terminalInput, userInputManager, threeWireService, argTransformer),
      sysInfoShell(terminalView, terminalInput, deviceView, userInputManager, argTransformer, systemService, littleFsService, wifiService),
      modbusShell(terminalView, terminalInput, utilityService, argTransformer, userInputManager, modbusService),
      oneWireEepromShell(terminalView, terminalInput, oneWireService, argTransformer, userInputManager, binaryAnalyzer),
      guideShell(terminalView, terminalInput, userInputManager),
      helpShell(terminalView, terminalInput, userInputManager),
      uartEmulationShell(terminalView, terminalInput, utilityService, uartService, argTransformer, userInputManager),
      profileShell(terminalView, terminalInput, utilityService, userInputManager, littleFsService, profileTransformer),
      cellCallShell(terminalView, terminalInput, utilityService, userInputManager, argTransformer, atTransformer, cellService),
      cellSmsShell(terminalView, terminalInput, userInputManager, argTransformer, atTransformer, cellService),
      fmBroadcastShell(terminalView, terminalInput, userInputManager, argTransformer, fmService),
      usbAdapterShell(terminalView, terminalInput, utilityService, userInputManager, nvsService, systemService),
      mouseShell(terminalView, terminalInput, userInputManager, utilityService),
      meshtasticShell(terminalView, terminalInput, utilityService, userInputManager,
                      argTransformer, loRaService, meshtasticService),

      // Selectors
      horizontalSelector(deviceView, deviceInput, utilityService),

      // Configurators
      terminalTypeConfigurator(horizontalSelector),

      // Controllers
      uartController(terminalView, terminalInput, deviceView, deviceInput, utilityService, uartService, sdService, hdUartService, uartSnifferService, argTransformer, userInputManager, uartAtShell, helpShell, uartEmulationShell),
      i2cController(terminalView, terminalInput, utilityService, i2cService, argTransformer, userInputManager, i2cEepromShell, helpShell),
      oneWireController(terminalView, terminalInput, utilityService, oneWireService, argTransformer, userInputManager, ibuttonShell, oneWireEepromShell, helpShell),
      infraredController(terminalView, terminalInput, deviceView, utilityService, infraredService, littleFsService, i2cService, argTransformer, infraredTransformer, userInputManager, universalRemoteShell, helpShell),
      utilityController(terminalView, deviceView, terminalInput, utilityService, pinService, i2sService, userInputManager, pinAnalyzer, aliasManager, argTransformer, commandTransformer, sysInfoShell, guideShell, helpShell, profileShell),
      hdUartController(terminalView, terminalInput, deviceInput, hdUartService, uartService, argTransformer, userInputManager, helpShell),
      spiController(terminalView, terminalInput, utilityService, spiService, sdService, argTransformer, userInputManager, binaryAnalyzer, sdCardShell, spiFlashShell, spiEepromShell, helpShell),
      jtagController(terminalView, terminalInput, jtagService, userInputManager, helpShell, usbAdapterShell),
      twoWireController(terminalView, terminalInput, userInputManager, twoWireService, smartCardShell, helpShell),
      threeWireController(terminalView, terminalInput, userInputManager, threeWireService, argTransformer, threeWireEepromShell, helpShell),
      dioController(terminalView, terminalInput, deviceView, utilityService, pinService, argTransformer, helpShell, userInputManager),
      ledController(terminalView, terminalInput, utilityService, ledService, argTransformer, userInputManager, helpShell),
      bluetoothController(terminalView, terminalInput, deviceInput, utilityService, bluetoothService, argTransformer, userInputManager, helpShell, mouseShell),
      i2sController(terminalView, terminalInput, utilityService, i2sService, argTransformer, userInputManager, helpShell),
      wifiController(terminalView, deviceView, terminalInput, deviceInput, utilityService, wifiService, wifiScannerService, ethernetService, sshService, netcatService, nmapService, icmpService, nvsService, httpService, telnetService, argTransformer, jsonTransformer, userInputManager, modbusShell, helpShell),
      canController(terminalView, terminalInput, userInputManager, utilityService, canService, argTransformer, helpShell),
      subGhzController(terminalView, terminalInput, deviceView, utilityService, subGhzService, pinService, i2sService, littleFsService, argTransformer, subGhzTransformer, userInputManager, subGhzAnalyzer, helpShell),
      rfidController(terminalView, terminalInput, utilityService, rfidService, userInputManager, argTransformer, helpShell),
      rf24Controller(terminalView, terminalInput, deviceView, utilityService, rf24Service, pinService, argTransformer, userInputManager, helpShell),
      ethernetController(terminalView, deviceView, terminalInput, deviceInput, utilityService, wifiService, wifiScannerService, ethernetService, sshService, netcatService, nmapService, icmpService, nvsService, httpService, telnetService, argTransformer, jsonTransformer, userInputManager, modbusShell, helpShell),
      usbController(terminalView, terminalInput, deviceInput, utilityService, usbService, argTransformer, userInputManager, helpShell, usbAdapterShell, mouseShell),
      cellController(terminalView, terminalInput, utilityService, cellService, argTransformer, atTransformer, userInputManager, helpShell, cellCallShell, cellSmsShell),
      fmController(terminalView, terminalInput, deviceView, utilityService, fmService, argTransformer, userInputManager, helpShell, fmBroadcastShell),
      loRaController(terminalView, terminalInput, deviceView, utilityService, loRaService, littleFsService, i2sService,
                     argTransformer, loRaTransformer,
                     commandTransformer, userInputManager, helpShell,
                     meshtasticShell),
      expanderController(terminalView, terminalInput, utilityService, uartService, argTransformer, userInputManager, helpShell)
{
}

// Accessors for core components
ITerminalView &DependencyProvider::getTerminalView() { return terminalView; }
void DependencyProvider::setTerminalView(ITerminalView &view) { terminalView = view; };
IDeviceView &DependencyProvider::getDeviceView() { return deviceView; }
IInput &DependencyProvider::getTerminalInput() { return terminalInput; }
IInput &DependencyProvider::getDeviceInput() { return deviceInput; }

// Services
SdService &DependencyProvider::getSdService() { return sdService; }
NvsService &DependencyProvider::getNvsService() { return nvsService; }
LedService &DependencyProvider::getLedService() { return ledService; }
I2cService &DependencyProvider::getI2cService() { return i2cService; }
UartService &DependencyProvider::getUartService() { return uartService; }
OneWireService &DependencyProvider::getOneWireService() { return oneWireService; }
TwoWireService &DependencyProvider::getTwoWireService() { return twoWireService; }
IInfraredService &DependencyProvider::getInfraredService() { return infraredService; }
UsbS3Service &DependencyProvider::getUsbService() { return usbService; }
SpiService &DependencyProvider::getSpiService() { return spiService; }
HdUartService &DependencyProvider::getHdUartService() { return hdUartService; }
PinService &DependencyProvider::getPinService() { return pinService; }
WifiService &DependencyProvider::getWifiService() { return wifiService; }
BluetoothService &DependencyProvider::getBluetoothService() { return bluetoothService; }
I2sService &DependencyProvider::getI2sService() { return i2sService; }
SshService &DependencyProvider::getSshService() { return sshService; }
NetcatService &DependencyProvider::getNetcatService() { return netcatService; }
NmapService &DependencyProvider::getNmapService() { return nmapService; }
ICMPService &DependencyProvider::getICMPService() { return icmpService; }
JtagService &DependencyProvider::getJtagService() { return jtagService; }
CanService &DependencyProvider::getCanService() { return canService; }
ModbusService &DependencyProvider::getModbusService() { return modbusService; }
SystemService &DependencyProvider::getSystemService() { return systemService; }
UtilityService &DependencyProvider::getUtilityService() { return utilityService; }
EthernetService &DependencyProvider::getEthernetService() { return ethernetService; }
SubGhzService &DependencyProvider::getSubGhzService() { return subGhzService; }
RfidService &DependencyProvider::getRfidService() { return rfidService; }
Rf24Service &DependencyProvider::getRf24Service() { return rf24Service; }
LittleFsService &DependencyProvider::getLittleFsService() { return littleFsService; }
CellService &DependencyProvider::getCellService() { return cellService; }
FmService &DependencyProvider::getFmService() { return fmService; }
LoRaService &DependencyProvider::getLoRaService() { return loRaService; }
MeshtasticService &DependencyProvider::getMeshtasticService() { return meshtasticService; }

// Controllers
UartController &DependencyProvider::getUartController() { return uartController; }
I2cController &DependencyProvider::getI2cController() { return i2cController; }
OneWireController &DependencyProvider::getOneWireController() { return oneWireController; }
UtilityController &DependencyProvider::getUtilityController() { return utilityController; }
InfraredController &DependencyProvider::getInfraredController() { return infraredController; }
UsbS3Controller &DependencyProvider::getUsbController() { return usbController; }
HdUartController &DependencyProvider::getHdUartController() { return hdUartController; }
SpiController &DependencyProvider::getSpiController() { return spiController; }
JtagController &DependencyProvider::getJtagController() { return jtagController; }
TwoWireController &DependencyProvider::getTwoWireController() { return twoWireController; }
ThreeWireController &DependencyProvider::getThreeWireController() { return threeWireController; }
DioController &DependencyProvider::getDioController() { return dioController; }
LedController &DependencyProvider::getLedController() { return ledController; }
WifiController &DependencyProvider::getWifiController() { return wifiController; }
BluetoothController &DependencyProvider::getBluetoothController() { return bluetoothController; }
I2sController &DependencyProvider::getI2sController() { return i2sController; }
CanController &DependencyProvider::getCanController() { return canController; }
EthernetController &DependencyProvider::getEthernetController() { return ethernetController; }
SubGhzController &DependencyProvider::getSubGhzController() { return subGhzController; }
RfidController &DependencyProvider::getRfidController() { return rfidController; }
Rf24Controller &DependencyProvider::getRf24Controller() { return rf24Controller; }
CellController &DependencyProvider::getCellController() { return cellController; }
FmController &DependencyProvider::getFmController() { return fmController; }
LoRaController &DependencyProvider::getLoRaController() { return loRaController; }
ExpanderController &DependencyProvider::getExpanderController() { return expanderController; }

// Transformers
TerminalCommandTransformer &DependencyProvider::getCommandTransformer() { return commandTransformer; }
InstructionTransformer &DependencyProvider::getInstructionTransformer() { return instructionTransformer; }
ArgTransformer &DependencyProvider::getArgTransformer() { return argTransformer; }
WebRequestTransformer &DependencyProvider::getWebRequestTransformer() { return webRequestTransformer; }
JsonTransformer &DependencyProvider::getJsonTransformer() { return jsonTransformer; }
AtTransformer &DependencyProvider::getAtTransformer() { return atTransformer; }
PinoutTransformer &DependencyProvider::getPinoutTransformer() { return pinoutTransformer; }

// Managers
CommandHistoryManager &DependencyProvider::getCommandHistoryManager() { return commandHistoryManager; }
CommandLineManager &DependencyProvider::getCommandLineManager() { return commandLineManager; }
UserInputManager &DependencyProvider::getUserInputManager() { return userInputManager; }
BinaryAnalyzer &DependencyProvider::getBinaryAnalyzer() { return binaryAnalyzer; }
SubGhzAnalyzer &DependencyProvider::getSubGhzAnalyzer() { return subGhzAnalyzer; }
PinAnalyzer &DependencyProvider::getPinAnalyzer() { return pinAnalyzer; }
AliasManager &DependencyProvider::getAliasManager() { return aliasManager; }

// Shells
SdCardShell &DependencyProvider::getSdCardShell() { return sdCardShell; }
UniversalRemoteShell &DependencyProvider::getUniversalRemoteShell() { return universalRemoteShell; }
I2cEepromShell &DependencyProvider::getI2cEepromShell() { return i2cEepromShell; }
SpiFlashShell &DependencyProvider::getSpiFlashShell() { return spiFlashShell; }
SpiEepromShell &DependencyProvider::getSpiEepromShell() { return spiEepromShell; }
SmartCardShell &DependencyProvider::getSmartCardShell() { return smartCardShell; }
ThreeWireEepromShell &DependencyProvider::getThreeWireEepromShell() { return threeWireEepromShell; }
IbuttonShell &DependencyProvider::getIbuttonShell() { return ibuttonShell; }
UartAtShell &DependencyProvider::getUartAtShell() { return uartAtShell; }
SysInfoShell &DependencyProvider::getSysInfoShell() { return sysInfoShell; }
ModbusShell &DependencyProvider::getModbusShell() { return modbusShell; }
OneWireEepromShell &DependencyProvider::getOneWireEepromShell() { return oneWireEepromShell; }
GuideShell &DependencyProvider::getGuideShell() { return guideShell; }
HelpShell &DependencyProvider::getHelpShell() { return helpShell; }
UartEmulationShell &DependencyProvider::getUartEmulationShell() { return uartEmulationShell; }
ProfileShell &DependencyProvider::getProfileShell() { return profileShell; }
CellCallShell &DependencyProvider::getCellCallShell() { return cellCallShell; }
CellSmsShell &DependencyProvider::getCellSmsShell() { return cellSmsShell; }
FmBroadcastShell &DependencyProvider::getFmBroadcastShell() { return fmBroadcastShell; }
UsbAdapterShell &DependencyProvider::getUsbAdapterShell() { return usbAdapterShell; }
MouseShell &DependencyProvider::getMouseShell() { return mouseShell; }
MeshtasticShell &DependencyProvider::getMeshtasticShell() { return meshtasticShell; }

// Selectors
HorizontalSelector &DependencyProvider::getHorizontalSelector() { return horizontalSelector; }

// Config
TerminalTypeConfigurator &DependencyProvider::getTerminalTypeConfigurator() { return terminalTypeConfigurator; }

// Disable interfaces
void DependencyProvider::disableAllProtocols()
{
  // getUartService().end();
  // getHdUartService().end();
  // getI2cService().end();
  // getSpiService().end();
  // getI2sService().end();
  // getTwoWireService().end();
}
