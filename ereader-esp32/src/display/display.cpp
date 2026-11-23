#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "display/display.h"

#define FULL_REFRESH_EVERY 20
static int partialCount = 0;

#define EPD_CS 11
#define EPD_DC 10
#define EPD_RST 9
#define EPD_BUSY 7

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> static display(GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2;

void displaySetFont(const uint8_t *font) { u8g2.setFont(font); }
void displayUseUiFont() { u8g2.setFont(u8g2_font_unifont_t_greek); }

void displayInit()
{
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(0, true, 2, false);
    display.setRotation(1);

    u8g2.begin(display);
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    u8g2.setFont(u8g2_font_unifont_t_greek);

    display.setFullWindow();
}

void screenSleep()
{
    display.powerOff();
    display.hibernate();
    Serial.println("Display is now in sleep mode.");
}

void displayRender(void (*draw)(), bool full)
{
    // if (full || partialCount >= FULL_REFRESH_EVERY)
    if (full)
    {
        display.setFullWindow();
        partialCount = 0;
    }
    else
    {
        display.setPartialWindow(0, 0, display.width(), display.height());
        partialCount++;
    }

    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        draw();
    } while (display.nextPage());
}

void displayText(int x, int y, const char *s)
{
    u8g2.setCursor(x, y);
    u8g2.print(s);
}

void displayRect(int x, int y, int w, int h)
{
    display.drawRect(x, y, w, h, GxEPD_BLACK);
    display.drawRect(x + 1, y + 1, w - 2, h - 2, GxEPD_BLACK); // 2 px thick
}

int displayWidth() { return display.width(); }
int displayHeight() { return display.height(); }
int displayTextWidth(const char *s) { return u8g2.getUTF8Width(s); }
int displayFontAscent() { return u8g2.getFontAscent(); }
int displayFontDescent() { return u8g2.getFontDescent(); }