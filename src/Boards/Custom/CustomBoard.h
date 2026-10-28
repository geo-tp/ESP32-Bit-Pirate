#pragma once

#ifdef DEVICE_CUSTOM

#include "Boards/Custom/CustomBoardConfig.h"
#include "Boards/Custom/CustomInput.h"
#include "Boards/Common/Serial/BoardHostSerial.h"

#if defined(CUSTOM_DISPLAY_DRIVER_ST7789_SPI)
#include "Boards/Common/Views/St7789SpiDeviceView.h"
#elif defined(CUSTOM_DISPLAY_DRIVER_ST7789_PARALLEL)
#include "Boards/Common/Views/St7789ParallelDeviceView.h"
#elif defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)
#include "Boards/Common/Views/Ili9341SpiDeviceView.h"
#else
#include "Boards/Common/Views/NoScreenDeviceView.h"
#endif

class CustomBoard final
{
public:
    CustomBoard();

    void initialize();
    IDeviceView &getDeviceView();
    IInput &getDeviceInput();
    IHostSerial &getHostSerial();

private:
#if defined(CUSTOM_DISPLAY_DRIVER_ST7789_SPI)
    static St7789SpiConfig createDisplayConfig();
    St7789SpiConfig displayConfig;
    St7789SpiDeviceView deviceView;
#elif defined(CUSTOM_DISPLAY_DRIVER_ST7789_PARALLEL)
    static St7789ParallelConfig createDisplayConfig();
    St7789ParallelConfig displayConfig;
    St7789ParallelDeviceView deviceView;
#elif defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)
    static Ili9341SpiConfig createDisplayConfig();
    Ili9341SpiConfig displayConfig;
    Ili9341SpiDeviceView deviceView;
#else
    NoScreenDeviceView deviceView;
#endif

    BoardHostSerial hostSerial;
    CustomInput deviceInput;
};

#endif
