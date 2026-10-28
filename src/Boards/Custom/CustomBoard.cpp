#ifdef DEVICE_CUSTOM

#include "Boards/Custom/CustomBoard.h"
#include <Arduino.h>

#if defined(CUSTOM_DISPLAY_DRIVER_ST7789_SPI)

St7789SpiConfig CustomBoard::createDisplayConfig()
{
    St7789SpiConfig config;

    config.pinBacklight = CUSTOM_DISPLAY_BACKLIGHT_PIN;
    config.pinMiso = CUSTOM_DISPLAY_SPI_MISO;
    config.pinMosi = CUSTOM_DISPLAY_SPI_MOSI;
    config.pinSclk = CUSTOM_DISPLAY_SPI_SCLK;
    config.pinCs = CUSTOM_DISPLAY_CS_PIN;
    config.pinDc = CUSTOM_DISPLAY_DC_PIN;
    config.pinReset = CUSTOM_DISPLAY_RESET_PIN;
    config.pinPower = CUSTOM_DISPLAY_POWER_PIN;

    config.panelWidth = CUSTOM_DISPLAY_PANEL_WIDTH;
    config.panelHeight = CUSTOM_DISPLAY_PANEL_HEIGHT;
    config.memoryWidth = CUSTOM_DISPLAY_MEMORY_WIDTH;
    config.memoryHeight = CUSTOM_DISPLAY_MEMORY_HEIGHT;
    config.offsetX = CUSTOM_DISPLAY_OFFSET_X;
    config.offsetY = CUSTOM_DISPLAY_OFFSET_Y;

    config.writeFrequency = CUSTOM_DISPLAY_WRITE_FREQ;
    config.readFrequency = CUSTOM_DISPLAY_SPI_READ_FREQ;
    config.rotation = CUSTOM_DISPLAY_ROTATION;
    config.invert = CUSTOM_DISPLAY_INVERT;
    config.rgbOrder = CUSTOM_DISPLAY_RGB_ORDER;
    config.powerActiveHigh = CUSTOM_DISPLAY_POWER_ACTIVE_HIGH;
    config.backlightActiveHigh = CUSTOM_DISPLAY_BACKLIGHT_ACTIVE_HIGH;
    config.useSharedSpi = CUSTOM_DISPLAY_SPI_USE_SHARED;

    return config;
}

CustomBoard::CustomBoard()
    : displayConfig(createDisplayConfig()),
      deviceView(displayConfig) {}

#elif defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)

Ili9341SpiConfig
CustomBoard::createDisplayConfig()
{
    Ili9341SpiConfig config;

    config.pinBacklight = CUSTOM_DISPLAY_BACKLIGHT_PIN;
    config.pinMiso = CUSTOM_DISPLAY_SPI_MISO;
    config.pinMosi = CUSTOM_DISPLAY_SPI_MOSI;
    config.pinSclk = CUSTOM_DISPLAY_SPI_SCLK;
    config.pinCs = CUSTOM_DISPLAY_CS_PIN;
    config.pinDc = CUSTOM_DISPLAY_DC_PIN;
    config.pinReset = CUSTOM_DISPLAY_RESET_PIN;
    config.pinPower = CUSTOM_DISPLAY_POWER_PIN;

    config.panelWidth = CUSTOM_DISPLAY_PANEL_WIDTH;
    config.panelHeight = CUSTOM_DISPLAY_PANEL_HEIGHT;
    config.memoryWidth = CUSTOM_DISPLAY_MEMORY_WIDTH;
    config.memoryHeight = CUSTOM_DISPLAY_MEMORY_HEIGHT;
    config.offsetX = CUSTOM_DISPLAY_OFFSET_X;
    config.offsetY = CUSTOM_DISPLAY_OFFSET_Y;

    config.writeFrequency = CUSTOM_DISPLAY_WRITE_FREQ;
    config.readFrequency = CUSTOM_DISPLAY_SPI_READ_FREQ;
    config.rotation = CUSTOM_DISPLAY_ROTATION;
    config.invert = CUSTOM_DISPLAY_INVERT;
    config.rgbOrder = CUSTOM_DISPLAY_RGB_ORDER;
    config.powerActiveHigh = CUSTOM_DISPLAY_POWER_ACTIVE_HIGH;
    config.backlightActiveHigh = CUSTOM_DISPLAY_BACKLIGHT_ACTIVE_HIGH;
    config.useSharedSpi = CUSTOM_DISPLAY_SPI_USE_SHARED;

    return config;
}

CustomBoard::CustomBoard()
    : displayConfig(createDisplayConfig()),
      deviceView(displayConfig) {}

#elif defined(CUSTOM_DISPLAY_DRIVER_ST7789_PARALLEL)

St7789ParallelConfig CustomBoard::createDisplayConfig()
{
    St7789ParallelConfig config;

    config.pinBacklight = CUSTOM_DISPLAY_BACKLIGHT_PIN;
    config.pinPower = CUSTOM_DISPLAY_POWER_PIN;

    config.pinWr = CUSTOM_DISPLAY_PARALLEL_WR;
    config.pinRd = CUSTOM_DISPLAY_PARALLEL_RD;
    config.pinDc = CUSTOM_DISPLAY_DC_PIN;
    config.pinD0 = CUSTOM_DISPLAY_PARALLEL_D0;
    config.pinD1 = CUSTOM_DISPLAY_PARALLEL_D1;
    config.pinD2 = CUSTOM_DISPLAY_PARALLEL_D2;
    config.pinD3 = CUSTOM_DISPLAY_PARALLEL_D3;
    config.pinD4 = CUSTOM_DISPLAY_PARALLEL_D4;
    config.pinD5 = CUSTOM_DISPLAY_PARALLEL_D5;
    config.pinD6 = CUSTOM_DISPLAY_PARALLEL_D6;
    config.pinD7 = CUSTOM_DISPLAY_PARALLEL_D7;

    config.pinCs = CUSTOM_DISPLAY_CS_PIN;
    config.pinReset = CUSTOM_DISPLAY_RESET_PIN;
    config.pinBusy = CUSTOM_DISPLAY_PARALLEL_BUSY;

    config.panelWidth = CUSTOM_DISPLAY_PANEL_WIDTH;
    config.panelHeight = CUSTOM_DISPLAY_PANEL_HEIGHT;
    config.memoryWidth = CUSTOM_DISPLAY_MEMORY_WIDTH;
    config.memoryHeight = CUSTOM_DISPLAY_MEMORY_HEIGHT;
    config.offsetX = CUSTOM_DISPLAY_OFFSET_X;
    config.offsetY = CUSTOM_DISPLAY_OFFSET_Y;

    config.writeFrequency = CUSTOM_DISPLAY_WRITE_FREQ;
    config.rotation = CUSTOM_DISPLAY_ROTATION;
    config.invert = CUSTOM_DISPLAY_INVERT;
    config.rgbOrder = CUSTOM_DISPLAY_RGB_ORDER;
    config.dlen16Bit = CUSTOM_DISPLAY_PARALLEL_DLEN_16BIT;
    config.powerActiveHigh = CUSTOM_DISPLAY_POWER_ACTIVE_HIGH;
    config.backlightActiveHigh = CUSTOM_DISPLAY_BACKLIGHT_ACTIVE_HIGH;

    config.selectionHelpLine1 = CUSTOM_DISPLAY_HELP_LINE_1;
    config.selectionHelpLine2 = CUSTOM_DISPLAY_HELP_LINE_2;

    return config;
}

CustomBoard::CustomBoard()
    : displayConfig(createDisplayConfig()),
      deviceView(displayConfig) {}

#else

CustomBoard::CustomBoard() = default;

#endif

void CustomBoard::initialize()
{
#if CUSTOM_BOARD_POWER_PIN >= 0
    pinMode(CUSTOM_BOARD_POWER_PIN, OUTPUT);
    digitalWrite(CUSTOM_BOARD_POWER_PIN, CUSTOM_BOARD_POWER_ACTIVE_HIGH ? HIGH : LOW);
#endif

    deviceView.initialize();

#if defined(CUSTOM_DISPLAY_DRIVER_ST7789_SPI) || defined(CUSTOM_DISPLAY_DRIVER_ST7789_PARALLEL) || defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)
    deviceView.logo();
    delay(900);
    deviceView.clear();
#endif
}

IDeviceView &CustomBoard::getDeviceView()
{
    return deviceView;
}

IInput &CustomBoard::getDeviceInput()
{
    return deviceInput;
}

IHostSerial &CustomBoard::getHostSerial()
{
    return hostSerial;
}

#endif
