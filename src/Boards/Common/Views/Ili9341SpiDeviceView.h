#pragma once

#if defined(DEVICE_CUSTOM) && defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)

#include "Interfaces/IDeviceView.h"
#include "States/GlobalState.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>

#define DARK_GREY_RECT 0x4208

// Hardware description for any SPI-connected ST7789 panel.
struct Ili9341SpiConfig
{
  int8_t pinBacklight = -1;
  int8_t pinMiso = -1;
  int8_t pinMosi = -1;
  int8_t pinSclk = -1;
  int8_t pinCs = -1;
  int8_t pinDc = -1;
  int8_t pinReset = -1;
  int8_t pinPower = -1;
  spi_host_device_t spiHost = SPI2_HOST;

  uint16_t panelWidth = 240;
  uint16_t panelHeight = 320;
  uint16_t memoryWidth = 240;
  uint16_t memoryHeight = 320;
  uint16_t offsetX = 35;
  uint16_t offsetY = 0;

  uint32_t writeFrequency = 10000000;
  uint32_t readFrequency = 10000000;
  uint8_t rotation = 3;
  bool invert = false;
  bool rgbOrder = false;
  bool powerActiveHigh = true;
  bool backlightActiveHigh = true;
  bool useSharedSpi = true;
  const char *selectionHelpLine1 = nullptr;
  const char *selectionHelpLine2 = nullptr;
};

// Lovyan driver
class LGFX_ILI9341SPI : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  explicit LGFX_ILI9341SPI(const Ili9341SpiConfig &displayConfig)
  {
    {
      auto cfg = _bus.config();

      cfg.spi_host = displayConfig.spiHost;
      cfg.spi_mode = 0;

      cfg.freq_write = displayConfig.writeFrequency;
      cfg.freq_read = displayConfig.readFrequency;

      cfg.pin_sclk = displayConfig.pinSclk;
      cfg.pin_mosi = displayConfig.pinMosi;
      cfg.pin_miso = displayConfig.pinMiso;
      cfg.pin_dc = displayConfig.pinDc;

      cfg.spi_3wire = false;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // --- PANEL
    {
      auto cfg = _panel.config();

      cfg.pin_cs = displayConfig.pinCs;
      cfg.pin_rst = displayConfig.pinReset;
      cfg.pin_busy = -1;

      cfg.panel_width = displayConfig.panelWidth;
      cfg.panel_height = displayConfig.panelHeight;
      cfg.memory_width = displayConfig.memoryWidth;
      cfg.memory_height = displayConfig.memoryHeight;
      cfg.offset_x = displayConfig.offsetX;
      cfg.offset_y = displayConfig.offsetY;

      cfg.invert = displayConfig.invert;
      cfg.rgb_order = displayConfig.rgbOrder;

      cfg.dlen_16bit = false;
      _panel.config(cfg);
    }

    setPanel(&_panel);
  }
};

class Ili9341SpiDeviceView : public IDeviceView
{
public:
  explicit Ili9341SpiDeviceView(const Ili9341SpiConfig &config);

  void initialize() override;
  SPIClass &getSharedSpiInstance() override;
  void *getScreen() override;
  void logo() override;
  void welcome(TerminalTypeEnum &terminalType, std::string &terminalInfos) override;
  void show(PinoutConfig &config) override;
  void loading() override;
  void adapterMode(const std::string &adapterName, const std::string &description, const std::vector<std::string> &details) override;
  void clear() override;
  void drawLogicTrace(uint8_t pin, const std::vector<uint8_t> &buffer, uint8_t step) override;
  void drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t> &buffer, uint8_t step) override;
  void drawWaterfall(const std::string &title, float startValue, float endValue, const char *unit, int rowIndex, int rowCount, int level) override;
  void setRotation(uint8_t rotation) override;
  void setBrightness(uint8_t brightness) override;
  uint8_t getBrightness() override;
  void topBar(const std::string &title, bool submenu, bool searchBar) override;
  void horizontalSelection(
      const std::vector<std::string> &options,
      uint16_t selectedIndex,
      const std::string &description1,
      const std::string &description2) override;

  void shutDown();

private:
  Ili9341SpiConfig config;
  LGFX_ILI9341SPI tft;
  //   lgfx::LGFX_Sprite canvas; // Not used currently, memory consumption is too high
  uint8_t brightnessPct = 100;
  SPIClass sharedSpi{HSPI};

  void drawCenterText(const std::string &text, int y, int fontSize);
  void welcomeWeb(const std::string &ip);
  void welcomeHotspot(const std::string &ip);
  void welcomeSerial(const std::string &baud);
};

#endif
