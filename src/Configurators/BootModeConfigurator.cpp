#include "Configurators/BootModeConfigurator.h"
#include "States/GlobalState.h"
#include "Adapters/SdrCdcAdapter.h"
#include "Managers/BootMemoryManager.h"
#include <algorithm>
#include <vector>

namespace {

void normalizeSumpLogicAnalyzerConfig(SumpLogicAnalyzerConfig& config) {
    constexpr uint8_t maxChannels = 8;

    uint8_t normalizedChannelCount = std::min<uint8_t>(config.channelCount, maxChannels);
    for (uint8_t i = normalizedChannelCount; i < maxChannels; ++i) {
        if (config.pins[i] != 0) {
            normalizedChannelCount = i + 1;
        }
    }

    config.channelCount = std::max<uint8_t>(normalizedChannelCount, 1);
}

}

BootModeConfigurator::BootModeConfigurator(IDeviceView& deviceView, IInput& deviceInput, NvsService& nvsService, IHostSerial& hostSerial)
    : deviceView(deviceView),
      deviceInput(deviceInput),
            nvsService(nvsService),
            hostSerial(hostSerial) {}

bool BootModeConfigurator::configure() {
    GlobalState& state = GlobalState::getInstance();
    UsbUartBridgeConfig usbUartBridgeConfig = {
        state.getUartRxPin(),
        state.getUartTxPin(),
        state.isUartInverted()
    };
    FlashromSerprogConfig flashromSerprogConfig = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMISOPin(),
        state.getSpiMOSIPin(),
        state.getSpiFrequency()
    };
    AvrDudeBusPirateConfig busPirateAvrdudeConfig = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMISOPin(),
        state.getSpiMOSIPin(),
        1000000
    };
    Bpio2AdapterConfig bpio2Config = {
        {
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMOSIPin(),
            state.getSpiMISOPin(),
            state.getI2cSclPin(),
            state.getI2cSdaPin(),
            state.getUartTxPin(),
            state.getUartRxPin()
        },
        state.getSpiFrequency(),
        state.getI2cFrequency()
    };
    SumpLogicAnalyzerConfig sumpLogicAnalyzerConfig = {
        {
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            0,
            0,
            0,
            0
        },
        4,
        1000000,
        4096
    };
    OpenOcdBusPirateConfig openOcdBusPirateConfig = {
        state.getSpiCLKPin(),
        state.getSpiCSPin(),
        state.getSpiMOSIPin(),
        state.getSpiMISOPin(),
        state.getSpiCLKPin(),
        state.getSpiMOSIPin()
    };
    InfraredToyConfig infraredToyConfig = {
        state.getInfraredTxPin(),
        state.getInfraredRxPin()
    };
    SubGhzRawCdcConfig subGhzRawCdcConfig = {
        state.getSubGhzSckPin(),
        state.getSubGhzMisoPin(),
        state.getSubGhzMosiPin(),
        state.getSubGhzCsPin(),
        state.getSubGhzGdoPin(),
        state.getSubGhzFrequency(),
        10,
        38400
    };

    nvsService.open();
    OneShotBootMode mode = nvsService.consumeOneShotBootMode();
    if (mode == OneShotBootMode::UsbUartBridge) {
        nvsService.getOneShotUsbUartBridgeConfig(
            state.getUartRxPin(),
            state.getUartTxPin(),
            state.isUartInverted(),
            usbUartBridgeConfig.uartRxPin,
            usbUartBridgeConfig.uartTxPin,
            usbUartBridgeConfig.uartInverted
        );
        nvsService.clearOneShotUsbUartBridgeConfig();
    }
    if (mode == OneShotBootMode::FlashromSerprog) {
        nvsService.getOneShotFlashromSerprogConfig(
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            state.getSpiFrequency(),
            flashromSerprogConfig.csPin,
            flashromSerprogConfig.sckPin,
            flashromSerprogConfig.misoPin,
            flashromSerprogConfig.mosiPin,
            flashromSerprogConfig.frequency,
            flashromSerprogConfig.wpPin,
            flashromSerprogConfig.holdPin
        );
        nvsService.clearOneShotFlashromSerprogConfig();
    }
    if (mode == OneShotBootMode::AvrDudeBusPirate) {
        nvsService.getOneShotAvrDudeBusPirateConfig(
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            1000000,
            busPirateAvrdudeConfig.csPin,
            busPirateAvrdudeConfig.sckPin,
            busPirateAvrdudeConfig.misoPin,
            busPirateAvrdudeConfig.mosiPin,
            busPirateAvrdudeConfig.frequency
        );
        nvsService.clearOneShotAvrDudeBusPirateConfig();
    }
    if (mode == OneShotBootMode::Bpio2) {
        nvsService.getOneShotBpio2Config(
            bpio2Config.ioPins,
            bpio2Config.ioPins,
            static_cast<uint8_t>(
                sizeof(bpio2Config.ioPins) / sizeof(bpio2Config.ioPins[0])
            )
        );
        nvsService.clearOneShotBpio2Config();
    }
    if (mode == OneShotBootMode::SumpLogicAnalyzer) {
        nvsService.getOneShotSumpLogicAnalyzerConfig(
            sumpLogicAnalyzerConfig.pins.data(),
            sumpLogicAnalyzerConfig.channelCount,
            sumpLogicAnalyzerConfig.channelCount
        );
        normalizeSumpLogicAnalyzerConfig(sumpLogicAnalyzerConfig);
        nvsService.clearOneShotSumpLogicAnalyzerConfig();
    }
    if (mode == OneShotBootMode::OpenOcdBusPirate) {
        nvsService.getOneShotOpenOcdBusPirateConfig(
            openOcdBusPirateConfig.tckPin,
            openOcdBusPirateConfig.tmsPin,
            openOcdBusPirateConfig.tdiPin,
            openOcdBusPirateConfig.tdoPin,
            openOcdBusPirateConfig.swclkPin,
            openOcdBusPirateConfig.swdioPin,
            openOcdBusPirateConfig.tckPin,
            openOcdBusPirateConfig.tmsPin,
            openOcdBusPirateConfig.tdiPin,
            openOcdBusPirateConfig.tdoPin,
            openOcdBusPirateConfig.swclkPin,
            openOcdBusPirateConfig.swdioPin
        );
        nvsService.clearOneShotOpenOcdBusPirateConfig();
    }
    if (mode == OneShotBootMode::InfraredToy) {
        nvsService.getOneShotInfraredToyConfig(
            state.getInfraredTxPin(),
            state.getInfraredRxPin(),
            infraredToyConfig.txPin,
            infraredToyConfig.rxPin
        );
        nvsService.clearOneShotInfraredToyConfig();
    }
    if (mode == OneShotBootMode::SubGhzRawCdc) {
        nvsService.getOneShotSubGhzRawCdcConfig(
            state.getSubGhzSckPin(),
            state.getSubGhzMisoPin(),
            state.getSubGhzMosiPin(),
            state.getSubGhzCsPin(),
            state.getSubGhzGdoPin(),
            state.getSubGhzFrequency(),
            10,
            38400,
            subGhzRawCdcConfig.sckPin,
            subGhzRawCdcConfig.misoPin,
            subGhzRawCdcConfig.mosiPin,
            subGhzRawCdcConfig.csPin,
            subGhzRawCdcConfig.gdo0Pin,
            subGhzRawCdcConfig.frequencyMhz,
            subGhzRawCdcConfig.paDbm,
            subGhzRawCdcConfig.baudrate
        );
        nvsService.clearOneShotSubGhzRawCdcConfig();
    }
    nvsService.close();

    // The consumed mode must agree with the heap ownership chosen before board initialization.
    ESP_ERROR_CHECK(BootMemoryManager::validateBootMode(mode));

    switch (mode) {
        case OneShotBootMode::SdrCdc:
            deviceView.adapterMode("ESP32-S3 SDR", "USB CDC baud 115200",
                                   {"2.2 to 2.8 GHz", "exp 100MHz-6GHz", "2.4 GHz radio", "raw I/Q"});
            SdrCdcAdapter::run(deviceInput, hostSerial);
            return true;
        case OneShotBootMode::UsbUartBridge:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            UsbUartBridgeAdapter::run(usbUartBridgeConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::FlashromSerprog:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            FlashromSerprogAdapter::run(flashromSerprogConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::AvrDudeBusPirate:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            AvrDudeBusPirateAdapter::run(busPirateAvrdudeConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::Bpio2:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            Bpio2Adapter::run(bpio2Config, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::SumpLogicAnalyzer:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            SumpLogicAnalyzerAdapter::run(sumpLogicAnalyzerConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::OpenOcdBusPirate:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            OpenOcdBusPirateAdapter::run(openOcdBusPirateConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::InfraredToy:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            InfraredToyAdapter::run(infraredToyConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::SubGhzRawCdc:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, bpio2Config, sumpLogicAnalyzerConfig, openOcdBusPirateConfig, infraredToyConfig, subGhzRawCdcConfig);
            SubGhzRawCdcAdapter::run(subGhzRawCdcConfig, deviceInput, hostSerial);
            return true;

        case OneShotBootMode::None:
        default:
            return false;
    }
}

void BootModeConfigurator::showOneShotBootMode(OneShotBootMode mode,
                                               const UsbUartBridgeConfig& usbUartBridgeConfig,
                                               const FlashromSerprogConfig& flashromSerprogConfig,
                                               const AvrDudeBusPirateConfig& busPirateAvrdudeConfig,
                                               const Bpio2AdapterConfig& bpio2Config,
                                               const SumpLogicAnalyzerConfig& sumpLogicAnalyzerConfig,
                                               const OpenOcdBusPirateConfig& openOcdBusPirateConfig,
                                               const InfraredToyConfig& infraredToyConfig,
                                               const SubGhzRawCdcConfig& subGhzRawCdcConfig) {
    switch (mode) {
        case OneShotBootMode::UsbUartBridge:
            deviceView.adapterMode(
                "USB-UART Bridge",
                "CDC serial bridge",
                {
                    "RX GPIO " + std::to_string(usbUartBridgeConfig.uartRxPin),
                    "TX GPIO " + std::to_string(usbUartBridgeConfig.uartTxPin)
                }
            );
            break;

        case OneShotBootMode::FlashromSerprog: {
            std::vector<std::string> details = {
                "CS GPIO " + std::to_string(flashromSerprogConfig.csPin),
                "SCK GPIO " + std::to_string(flashromSerprogConfig.sckPin),
                "MISO GPIO " + std::to_string(flashromSerprogConfig.misoPin),
                "MOSI GPIO " + std::to_string(flashromSerprogConfig.mosiPin)
            };
            if (flashromSerprogConfig.wpPin >= 0)
                details.push_back("WP GPIO " + std::to_string(flashromSerprogConfig.wpPin));
            if (flashromSerprogConfig.holdPin >= 0)
                details.push_back("HOLD GPIO " + std::to_string(flashromSerprogConfig.holdPin));
            deviceView.adapterMode("Flashrom SPI", "serprog SPI programmer", details);
            break;
        }

        case OneShotBootMode::AvrDudeBusPirate:
            deviceView.adapterMode(
                "AVRDUDE Bus Pirate",
                "AVR ISP binary SPI",
                {
                    "RESET/CS GPIO " + std::to_string(busPirateAvrdudeConfig.csPin),
                    "SCK GPIO " + std::to_string(busPirateAvrdudeConfig.sckPin),
                    "MISO GPIO " + std::to_string(busPirateAvrdudeConfig.misoPin),
                    "MOSI GPIO " + std::to_string(busPirateAvrdudeConfig.mosiPin),
                    "SCK " + std::to_string(busPirateAvrdudeConfig.frequency / 1000) + " kHz"
                }
            );
            break;

        case OneShotBootMode::Bpio2: {
            std::vector<std::string> details;
            details.reserve(8);
            for (size_t i = 0; i < BPIO2_IO_PIN_COUNT; ++i) {
                details.push_back(
                    "IO" + std::to_string(i) + " GPIO " + std::to_string(bpio2Config.ioPins[i])
                );
            }
            deviceView.adapterMode(
                "BPIO2",
                "GPIO / SPI / I2C",
                details
            );
            break;
        }

        case OneShotBootMode::SumpLogicAnalyzer: {
            std::vector<std::string> details;
            uint8_t channelCount = std::min<uint8_t>(sumpLogicAnalyzerConfig.channelCount, 8);
            details.reserve(channelCount);

            for (uint8_t i = 0; i < channelCount; ++i) {
                details.push_back(
                    "D" + std::to_string(i) + " GPIO " + std::to_string(sumpLogicAnalyzerConfig.pins[i])
                );
            }

            deviceView.adapterMode(
                "SUMP Logic",
                "PulseView/Sigrok OLS",
                details
            );
            break;
        }

        case OneShotBootMode::OpenOcdBusPirate:
            deviceView.adapterMode(
                "OpenOCD",
                "Bus Pirate JTAG/SWD",
                {
                    "TCK GPIO " + std::to_string(openOcdBusPirateConfig.tckPin),
                    "TMS GPIO " + std::to_string(openOcdBusPirateConfig.tmsPin),
                    "TDI GPIO " + std::to_string(openOcdBusPirateConfig.tdiPin),
                    "TDO GPIO " + std::to_string(openOcdBusPirateConfig.tdoPin),
                    "SWCLK GPIO " + std::to_string(openOcdBusPirateConfig.swclkPin),
                    "SWDIO GPIO " + std::to_string(openOcdBusPirateConfig.swdioPin)
                }
            );
            break;

        case OneShotBootMode::InfraredToy:
            deviceView.adapterMode(
                "USB IR Toy",
                "LIRC raw IR adapter",
                {
                    "IR TX GPIO " + std::to_string(infraredToyConfig.txPin),
                    "IR RX GPIO " + std::to_string(infraredToyConfig.rxPin)
                }
            );
            break;

        case OneShotBootMode::SubGhzRawCdc:
            deviceView.adapterMode(
                "SubGHz Raw CDC",
                "CC1101 RAW OOK CDC",
                {
                    "SCK GPIO " + std::to_string(subGhzRawCdcConfig.sckPin),
                    "MISO GPIO " + std::to_string(subGhzRawCdcConfig.misoPin),
                    "MOSI GPIO " + std::to_string(subGhzRawCdcConfig.mosiPin),
                    "CS GPIO " + std::to_string(subGhzRawCdcConfig.csPin),
                    "GDO0 GPIO " + std::to_string(subGhzRawCdcConfig.gdo0Pin),
                    "Baud " + std::to_string(subGhzRawCdcConfig.baudrate)
                }
            );
            break;

        case OneShotBootMode::None:
        default:
            break;
    }
}
