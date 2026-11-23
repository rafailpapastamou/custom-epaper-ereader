#include "display/display.h"
#include "screens/settings.h"
#include "screens/reader.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <string>
#include <cstdio>

static const int HEADER_H = 30;
static const int FOOTER_H = 30;
static const int PAD = 8;

// ---------------- Fonts ----------------
// NOTE: the names below must exist in your u8g2 version and contain Greek
// glyphs. Only u8g2_font_unifont_t_greek is confirmed by your code, so if one
// of the others doesn't compile, swap it for one from the u8g2 font list.

struct FontSize
{
    const char *label;
    const uint8_t *font;
};
struct FontFamily
{
    const char *name;
    const FontSize *sizes;
    int count;
};

static const FontSize UNIFONT[] = {{"16", u8g2_font_unifont_t_greek}};
static const FontSize FIXED[] = {
    "20", u8g2_font_10x20_t_greek};

static const FontFamily FAMILIES[] = {
    {"Unifont", UNIFONT, 1},
    {"Fixed", FIXED, 3},
};
static const int FAMILY_COUNT = sizeof(FAMILIES) / sizeof(FAMILIES[0]);

static int fontFamily = 0;
static int fontSize = 0;
static bool fontDirty = false; // font changed, current chapter needs re-paginating

const uint8_t *settingsReadingFont()
{
    return FAMILIES[fontFamily].sizes[fontSize].font;
}

// ---------------- Persistence ----------------

static const char *SETTINGS_FILE = "/sd/settings.txt";

void settingsInit()
{
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f)
        return;
    int a = 0, b = 0;
    if (fscanf(f, "%d %d", &a, &b) == 2 && a >= 0 && a < FAMILY_COUNT &&
        b >= 0 && b < FAMILIES[a].count)
    {
        fontFamily = a;
        fontSize = b;
    }
    fclose(f);
}

static void saveSettings()
{
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f)
        return;
    fprintf(f, "%d %d\n", fontFamily, fontSize);
    fclose(f);
}

// ---------------- Menu state ----------------

enum Page
{
    PAGE_MAIN,
    PAGE_FONT,
    PAGE_SIZE,
    PAGE_CHAPTER
};

static Page page = PAGE_MAIN;
static int sel = 0;
static int mainRow = 0; // row to return to on the main page

static const int MAIN_ROWS = 5; // Resume, Font, Font size, Chapter, Library

static int itemCount()
{
    switch (page)
    {
    case PAGE_MAIN:
        return MAIN_ROWS;
    case PAGE_FONT:
        return FAMILY_COUNT;
    case PAGE_SIZE:
        return FAMILIES[fontFamily].count;
    case PAGE_CHAPTER:
        return readerTocCount();
    }
    return 0;
}

static const char *pageTitle()
{
    switch (page)
    {
    case PAGE_FONT:
        return "Font";
    case PAGE_SIZE:
        return "Font size";
    case PAGE_CHAPTER:
        return "Chapter";
    default:
        return "Settings";
    }
}

static std::string itemLabel(int i)
{
    char buf[64];
    switch (page)
    {
    case PAGE_MAIN:
        switch (i)
        {
        case 0:
            return "Resume";
        case 1:
            snprintf(buf, sizeof(buf), "< Font (%s) >", FAMILIES[fontFamily].name);
            return buf;
        case 2:
            snprintf(buf, sizeof(buf), "< Font size (%s) >",
                     FAMILIES[fontFamily].sizes[fontSize].label);
            return buf;
        case 3:
            return "< Chapter >";
        default:
            return "<- Library";
        }
    case PAGE_FONT:
        return std::string(i == fontFamily ? "* " : "  ") + FAMILIES[i].name;
    case PAGE_SIZE:
        return std::string(i == fontSize ? "* " : "  ") + FAMILIES[fontFamily].sizes[i].label;
    case PAGE_CHAPTER:
    {
        std::string t = readerTocTitle(i);
        if (t.empty())
        {
            snprintf(buf, sizeof(buf), "Chapter %d", i + 1);
            t = buf;
        }
        return std::string(i == readerCurrentTocIndex() ? "* " : "  ") + t;
    }
    }
    return "";
}

static void fitToWidth(std::string &s, int maxW)
{
    while (!s.empty() && displayTextWidth(s.c_str()) > maxW)
    {
        size_t pos = s.size() - 1;
        while (pos > 0 && (((unsigned char)s[pos]) & 0xC0) == 0x80)
            pos--;
        s.resize(pos);
    }
}

// ---------------- Drawing ----------------

static void drawSettings()
{
    displayUseUiFont();
    displayText(10, 22, pageTitle());

    int n = itemCount();
    if (n == 0)
    {
        displayText(10, HEADER_H + 30, "No chapters");
        return;
    }

    int rowH = displayFontAscent() - displayFontDescent() + PAD;
    int rows = (displayHeight() - HEADER_H - FOOTER_H) / rowH;
    if (rows < 1)
        rows = 1;

    int first = (sel / rows) * rows;
    int maxW = displayWidth() - 2 * 14;

    for (int i = 0; i < rows && first + i < n; i++)
    {
        int idx = first + i;
        int top = HEADER_H + i * rowH;

        std::string label = itemLabel(idx);
        fitToWidth(label, maxW);
        displayText(14, top + PAD / 2 + displayFontAscent(), label.c_str());

        if (idx == sel)
            displayRect(4, top, displayWidth() - 8, rowH);
    }

    if (n > rows)
    {
        // Increased buffer size from 16 to 32 to fix format truncation warning under -Werror
        char footer[32];
        snprintf(footer, sizeof(footer), "%d/%d", sel / rows + 1, (n + rows - 1) / rows);
        displayText(10, displayHeight() - 8, footer);
    }
}

// ---------------- Public functions ----------------

void showSettingsMenu()
{
    page = PAGE_MAIN;
    sel = 0;
    displayRender(drawSettings, true);
}

void settingsUp()
{
    int n = itemCount();
    if (n == 0)
        return;
    sel = (sel - 1 + n) % n;
    displayRender(drawSettings, false);
}

void settingsDown()
{
    int n = itemCount();
    if (n == 0)
        return;
    sel = (sel + 1) % n;
    displayRender(drawSettings, false);
}

static void openPage(Page p, int startSel)
{
    mainRow = sel;
    page = p;
    sel = startSel;
    if (sel < 0 || sel >= itemCount())
        sel = 0;
    displayRender(drawSettings, false);
}

static void backToMain()
{
    page = PAGE_MAIN;
    sel = mainRow;
    displayRender(drawSettings, false);
}

static SettingsAction resume()
{
    if (fontDirty)
    {
        readerReflow(); // re-paginate the current chapter with the new font
        fontDirty = false;
    }
    return SETTINGS_RESUME;
}

SettingsAction settingsSelect()
{
    switch (page)
    {
    case PAGE_MAIN:
        switch (sel)
        {
        case 0:
            return resume();
        case 1:
            openPage(PAGE_FONT, fontFamily);
            break;
        case 2:
            openPage(PAGE_SIZE, fontSize);
            break;
        case 3:
            if (readerTocCount() > 0)
                openPage(PAGE_CHAPTER, readerCurrentTocIndex());
            break;
        case 4:
            return SETTINGS_LIBRARY;
        }
        break;

    case PAGE_FONT:
        if (sel != fontFamily)
        {
            fontFamily = sel;
            if (fontSize >= FAMILIES[fontFamily].count)
                fontSize = FAMILIES[fontFamily].count - 1;
            fontDirty = true;
            saveSettings();
        }
        backToMain();
        break;

    case PAGE_SIZE:
        if (sel != fontSize)
        {
            fontSize = sel;
            fontDirty = true;
            saveSettings();
        }
        backToMain();
        break;

    case PAGE_CHAPTER:
    {
        int chosen = sel;
        fontDirty = false; // the chapter is loaded with the new font anyway
        page = PAGE_MAIN;
        sel = 0;
        readerOpenTocEntry(chosen); // loads the chapter and draws it
        return SETTINGS_RESUMED;
    }
    }

    return SETTINGS_STAY;
}

SettingsAction settingsBack()
{
    if (page == PAGE_MAIN)
        return resume();
    backToMain();
    return SETTINGS_STAY;
}