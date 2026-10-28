#if defined(DEVICE_CUSTOM) && defined(CUSTOM_DISPLAY_DRIVER_ILI9341_SPI)

#include "Boards/Common/Views/Ili9341SpiDeviceView.h"
#include "Data/WelcomeScreen.h"

#include <algorithm>

Ili9341SpiDeviceView::Ili9341SpiDeviceView(const Ili9341SpiConfig &displayConfig)
    : config(displayConfig), tft(config)
{
  if (config.pinPower >= 0)
  {
    pinMode(config.pinPower, OUTPUT);
    digitalWrite(config.pinPower, config.powerActiveHigh ? HIGH : LOW);
  }

  if (config.pinBacklight >= 0)
  {
    pinMode(config.pinBacklight, OUTPUT);
    digitalWrite(config.pinBacklight, config.backlightActiveHigh ? HIGH : LOW);
  }
}

SPIClass &Ili9341SpiDeviceView::getSharedSpiInstance()
{
  return config.useSharedSpi ? sharedSpi : SPI;
}

void *Ili9341SpiDeviceView::getScreen()
{
  return &tft;
}

void Ili9341SpiDeviceView::initialize()
{
  if (config.pinPower >= 0)
  {
    pinMode(config.pinPower, OUTPUT);
    digitalWrite(config.pinPower, config.powerActiveHigh ? HIGH : LOW);
  }

  tft.init();
  tft.setRotation(config.rotation);
  tft.setSwapBytes(true);

  if (config.pinBacklight >= 0)
  {
    pinMode(config.pinBacklight, OUTPUT);
    digitalWrite(config.pinBacklight, config.backlightActiveHigh ? HIGH : LOW);
  }

  setBrightness(brightnessPct);

  tft.fillScreen(TFT_BLACK);
  //  tft.drawLine(0, 0, 200, 200, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void Ili9341SpiDeviceView::logo()
{
  clear();

  // Logo
  tft.setSwapBytes(true);
  const int displayWidth = static_cast<int>(tft.width());
  const int displayHeight = static_cast<int>(tft.height());
  const int logoX = std::max(0, (displayWidth - WELCOME_IMAGE_WIDTH) / 2);
  const int logoY = std::max(0, std::min(displayHeight * 30 / 170, displayHeight - WELCOME_IMAGE_HEIGHT));
  tft.pushImage(logoX, logoY, WELCOME_IMAGE_WIDTH, WELCOME_IMAGE_HEIGHT, WelcomeScreen);
  tft.setSwapBytes(false);

  // Sub
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  GlobalState &state = GlobalState::getInstance();
  auto version = std::string("ESP32 Bit Pirate - ") + state.getVersion();
  drawCenterText(version.c_str(), tft.height() * 130 / 170, 2);
}

void Ili9341SpiDeviceView::welcome(TerminalTypeEnum &terminalType, std::string &terminalInfos)
{
  if (terminalType == TerminalTypeEnum::WiFiAp)
    welcomeHotspot(terminalInfos);
  else if (terminalType == TerminalTypeEnum::WiFiClient)
    welcomeWeb(terminalInfos);
  else
    welcomeSerial(terminalInfos);
}

void Ili9341SpiDeviceView::loading()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);

  tft.fillRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, DARK_GREY_RECT);
  tft.drawRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, TFT_GREEN);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Loading...", tft.width() / 2, tft.height() / 2);
  tft.setTextDatum(TL_DATUM);
}

void Ili9341SpiDeviceView::clear()
{
  tft.fillScreen(TFT_BLACK);
}

void Ili9341SpiDeviceView::drawLogicTrace(uint8_t pin, const std::vector<uint8_t> &buffer, uint8_t step)
{
  tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("GPIO ");
  tft.print(pin);

  const int traceY = 50;
  const int traceH = 80;
  const int centerY = traceY + traceH / 2;

  int x = 10;
  for (size_t i = 1; i < buffer.size(); ++i)
  {
    uint8_t prev = buffer[i - 1];
    uint8_t curr = buffer[i];

    int y1 = prev ? (centerY - 15) : (centerY + 15);
    int y2 = curr ? (centerY - 15) : (centerY + 15);

    if (curr != prev)
    {
      tft.drawLine(x, y1, x + step, y1, prev ? TFT_GREEN : TFT_WHITE);
      tft.drawLine(x + step, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
    }
    else
    {
      tft.drawLine(x, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
    }

    x += step;
    if (x > tft.width() - step)
      break;
  }
}

void Ili9341SpiDeviceView::drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t> &buffer, uint8_t step)
{
  tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("GPIO ");
  tft.print(pin);

  const int topY = 35;
  const int h = tft.height() - topY;

  int x = 10;
  for (size_t i = 1; i < buffer.size(); ++i)
  {
    int prev = topY + (h - 1) - (((buffer[i - 1] >> 1) * (h - 1)) / 134);
    int curr = topY + (h - 1) - (((buffer[i] >> 1) * (h - 1)) / 134);
    tft.drawLine(x, prev, x + step, curr, TFT_GREEN);
    x += step;
    if (x > tft.width() - step)
      break;
  }
}

void Ili9341SpiDeviceView::drawWaterfall(
    const std::string &title,
    float startValue,
    float endValue,
    const char *unit,
    int rowIndex,
    int rowCount,
    int level)
{
  const int W = tft.width();
  const int H = tft.height();
  const int midX = W / 2;

  const int headerH = 12;
  const int footerH = 12;
  const int graphY = headerH;
  const int graphH = H - headerH - footerH;

  const int barMaxPixels = midX - 2;

  // Clamp level
  if (level < 0)
    level = 0;
  if (level > 100)
    level = 100;
  int barPixels = (level * barMaxPixels) / 100;

  // First row: titles, labels
  if (rowIndex == 0)
  {
    // Full clear
    tft.fillScreen(TFT_BLACK);

    // Title
    tft.setTextSize(1);
    tft.setTextFont(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(2, 2);
    tft.print(title.c_str());

    // Format labels
    char bufStart[24];
    char bufEnd[24];

    if (unit && unit[0])
    {
      snprintf(bufStart, sizeof(bufStart), "%.2f%s", startValue, unit);
      snprintf(bufEnd, sizeof(bufEnd), "%.2f%s", endValue, unit);
    }
    else
    {
      snprintf(bufStart, sizeof(bufStart), "%.2f", startValue);
      snprintf(bufEnd, sizeof(bufEnd), "%.2f", endValue);
    }

    // Start label (top right)
    int wStart = tft.textWidth(bufStart);
    tft.setCursor(W - wStart - 2, 2);
    tft.print(bufStart);

    // End label (bottom right)
    int wEnd = tft.textWidth(bufEnd);
    tft.setCursor(W - wEnd - 2, H - footerH + 2);
    tft.print(bufEnd);

    // Graph area
    tft.fillRect(0, graphY, W, graphH, TFT_BLACK);
    tft.drawFastVLine(midX, graphY, graphH, TFT_DARKGREY);
  }

  if (rowCount <= 1)
    return;
  if (rowIndex < 0)
    rowIndex = 0;
  if (rowIndex > rowCount - 1)
    rowIndex = rowCount - 1;

  // Map row to Y
  int y = graphY + (int)((int64_t)rowIndex * (graphH - 1) / (rowCount - 1));

  // Clear this row only
  tft.drawFastHLine(0, y, W, TFT_BLACK);

  // Restore center pixel
  tft.drawPixel(midX, y, TFT_DARKGREY);

  // Draw energy bar
  if (barPixels > 0)
  {
    int x0 = midX - barPixels;
    int w = barPixels * 2;

    if (x0 < 0)
    {
      w += x0;
      x0 = 0;
    }
    if (x0 + w > W)
      w = W - x0;

    if (w > 0)
    {
      tft.drawFastHLine(x0, y, w, TFT_GREEN);
    }
  }
}

void Ili9341SpiDeviceView::setRotation(uint8_t rotation)
{
  tft.setRotation(rotation);
}

void Ili9341SpiDeviceView::setBrightness(uint8_t brightness)
{
  if (brightness > 100)
    brightness = 100;
  brightnessPct = brightness;

  uint8_t pwm = (uint8_t)((brightnessPct * 255) / 100);
  tft.setBrightness(pwm);
}

uint8_t Ili9341SpiDeviceView::getBrightness()
{
  return brightnessPct;
}

void Ili9341SpiDeviceView::topBar(const std::string &title, bool submenu, bool searchBar)
{
  (void)submenu;
  (void)searchBar;

  // Zone topbar
  tft.fillRect(0, 0, tft.width(), 30, TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title.c_str(), tft.width() / 2, 20);
  tft.setTextDatum(TL_DATUM);
}

void Ili9341SpiDeviceView::horizontalSelection(
    const std::vector<std::string> &options,
    uint16_t selectedIndex,
    const std::string &description1,
    const std::string &description2)
{
  const int screenW = tft.width();
  const int screenH = tft.height();
  const int originY = screenH * 30 / 170;

  // Box option
  const std::string &option = options[selectedIndex];
  int boxX = screenW * 60 / 320;
  int boxW = screenW - boxX * 2;
  int boxY = originY + screenH * 45 / 170;
  int boxH = screenH * 50 / 170;
  int corner = std::min(8, boxH / 2);

  // Description 1
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(description1.c_str(), screenW / 2, originY + screenH * 26 / 170);

  // Description 2
  const char *helpLine1 = config.selectionHelpLine1 ? config.selectionHelpLine1 : description2.c_str();
  const char *helpLine2 = config.selectionHelpLine2;

  tft.setTextColor(DARK_GREY_RECT, TFT_BLACK);
  if (helpLine2 && helpLine2[0])
  {
    tft.drawString(helpLine1, screenW / 2, screenH - screenH * 30 / 170);
    tft.drawString(helpLine2, screenW / 2, screenH - screenH * 12 / 170);
  }
  else if (helpLine1 && helpLine1[0])
  {
    tft.drawString(helpLine1, screenW / 2, screenH - screenH * 20 / 170);
  }

  // Box background + border
  tft.fillRoundRect(boxX, boxY, boxW, boxH, corner, DARK_GREY_RECT);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, corner, TFT_GREEN);

  const int pad = 4;
  int innerX = boxX + pad;
  int innerY = boxY + pad;
  int innerW = boxW - (pad * 2);
  int innerH = boxH - (pad * 2);
  int innerCorner = corner - pad;
  if (innerCorner < 0)
    innerCorner = 0;

  // erase old text
  tft.fillRoundRect(innerX, innerY, innerW, innerH, innerCorner, DARK_GREY_RECT);

  // Option name
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setTextFont(2);
  tft.setTextSize(2);

  int textW = tft.textWidth(option.c_str());
  int textX = (screenW - textW) / 2;
  int textH = tft.fontHeight();
  int textY = boxY + (boxH - textH) / 2;

  tft.setTextDatum(TL_DATUM);
  tft.drawString(option.c_str(), textX, textY);

  // Arrows
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(boxX / 2, boxY + (boxH - tft.fontHeight()) / 2);
  tft.print("<");
  tft.setCursor(screenW - boxX / 2 - tft.textWidth(">"), boxY + (boxH - tft.fontHeight()) / 2);
  tft.print(">");
}

void Ili9341SpiDeviceView::drawCenterText(const std::string &text, int y, int fontSize)
{
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(fontSize);
  tft.drawString(text.c_str(), tft.width() / 2, y);
  tft.setTextDatum(TL_DATUM);
}

void Ili9341SpiDeviceView::welcomeSerial(const std::string &baudStr)
{
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  drawCenterText("Open Serial (USB COM)", tft.height() * 35 / 170, 2);

  // Rect baudrate
  const int boxX = tft.width() * 70 / 320;
  const int boxY = tft.height() * 60 / 170;
  const int boxW = tft.width() - boxX * 2;
  const int boxH = tft.height() * 40 / 170;
  tft.fillRoundRect(boxX, boxY, boxW, boxH, 8, DARK_GREY_RECT);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_GREEN);

  // Texte baud
  std::string baud = "Baudrate: " + baudStr;
  int textW = tft.textWidth(baud.c_str());
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setCursor((tft.width() - textW) / 2, boxY + (boxH - tft.fontHeight()) / 2);
  tft.print(baud.c_str());

  // Sub
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Then press any key in terminal", tft.width() / 2, boxY + boxH + tft.height() * 7 / 170);
  tft.setTextDatum(TL_DATUM);
}

void Ili9341SpiDeviceView::welcomeWeb(const std::string &ipStr)
{
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  drawCenterText("Open browser to connect", tft.height() * 34 / 170, 2);

  // Rectangle IP
  const int boxX = tft.width() * 60 / 320;
  const int boxY = tft.height() * 60 / 170;
  const int boxW = tft.width() - boxX * 2;
  const int boxH = tft.height() * 40 / 170;
  tft.fillRoundRect(boxX, boxY, boxW, boxH, 8, DARK_GREY_RECT);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_GREEN);

  // Texte IP
  std::string ip = "http://" + ipStr;
  int textW = tft.textWidth(ip.c_str());
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setCursor((tft.width() - textW) / 2, boxY + (boxH - tft.fontHeight()) / 2);
  tft.print(ip.c_str());
}

void Ili9341SpiDeviceView::welcomeHotspot(const std::string &ipStr)
{
  GlobalState &state = GlobalState::getInstance();
  PinoutConfig config;
  config.setMode("HOTSPOT");
  config.setMappings({state.getActiveApName(),
                      std::string("PW ") + state.getApPassword(),
                      std::string("IP ") + ipStr,
                      "CONNECT TO AP"});
  show(config);
}

void Ili9341SpiDeviceView::adapterMode(const std::string &adapterName, const std::string &description, const std::vector<std::string> &details)
{
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(adapterName.c_str(), tft.width() / 2, 28);

  tft.setTextSize(1);
  const int screenW = tft.width();
  const int screenH = tft.height();
  size_t detailCount = std::min<size_t>(details.size(), 8);
  size_t detailRows = (detailCount + 1) / 2;
  int compactLayoutOffsetY = detailRows > 2 ? -4 : 0;
  for (size_t i = 0; i < detailCount; ++i)
  {
    int col = i % 2;
    int row = i / 2;
    int marginX = screenW * 18 / 320;
    int gapX = screenW * 8 / 320;
    int boxW = (screenW - marginX * 2 - gapX) / 2;
    int boxH = screenH * 19 / 170;
    int boxX = marginX + col * (boxW + gapX);
    int boxY = screenH * (53 + row * 21 + compactLayoutOffsetY) / 170;
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, DARK_GREY_RECT);
    tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
    tft.drawString(details[i].c_str(), boxX + (boxW / 2), boxY + (boxH / 2));
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int descY = screenH * ((detailRows > 2 ? 148 : 112) + compactLayoutOffsetY) / 170;
  tft.drawString(description.c_str(), screenW / 2, descY);
  tft.setTextColor(0xC618, TFT_BLACK);
  tft.drawString("Press any button to return", screenW / 2, descY + screenH * 16 / 170);
  tft.setTextDatum(TL_DATUM);
}

void Ili9341SpiDeviceView::shutDown()
{
  tft.setRotation(config.rotation);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  drawCenterText("Shutting down...", 80, 2);
  delay(1000);

  if (config.pinBacklight >= 0)
  {
    digitalWrite(config.pinBacklight, config.backlightActiveHigh ? LOW : HIGH);
  }
  if (config.pinPower >= 0)
  {
    digitalWrite(config.pinPower, config.powerActiveHigh ? LOW : HIGH);
  }
}

void Ili9341SpiDeviceView::show(PinoutConfig &config)
{
  tft.fillScreen(TFT_BLACK);

  const auto &mappings = config.getMappings();
  auto mode = config.getMode();

  // Mode name
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  std::string modeStr = "MODE " + mode;
  tft.drawString(modeStr.c_str(), tft.width() / 2, 20);
  tft.setTextDatum(TL_DATUM);

  // No mapping
  if (mappings.empty())
  {
    const int frameX = 20;
    const int frameY = 45;
    const int frameW = tft.width() - 40;
    const int frameH = tft.height() - 70;
    const int frameR = 5;

    tft.fillRoundRect(frameX, frameY, frameW, frameH, frameR, TFT_BLACK);
    tft.drawRoundRect(frameX, frameY, frameW, frameH, frameR, TFT_GREEN);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Nothing to display", tft.width() / 2, frameY + frameH / 2);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  // Mapping list
  int startY = tft.height() * 40 / 170;
  int gap = std::max(1, static_cast<int>(tft.height() * 4 / 170));
  int availableHeight = tft.height() - startY;
  int boxHeight = std::min(24, (availableHeight - gap * ((int)mappings.size() - 1)) / (int)mappings.size());

  for (size_t i = 0; i < mappings.size(); ++i)
  {
    int y = startY + (int)i * (boxHeight + gap);

    tft.fillRoundRect(20, y, tft.width() - 40, boxHeight, 6, DARK_GREY_RECT);
    tft.drawRoundRect(20, y, tft.width() - 40, boxHeight, 6, TFT_GREEN);

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);

    int w = tft.textWidth(mappings[i].c_str());
    int textX = (tft.width() - w) / 2;

    tft.setCursor(textX, y + 5);
    tft.print(mappings[i].c_str());
  }
}

#endif
