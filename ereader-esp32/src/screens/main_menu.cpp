#include "screens/main_menu.h"
#include "epub/sd_card.h"
#include "display/display.h"

static std::vector<Book> files;
static int selectedIndex = 0;

static const int HEADER_H = 30;
static const int FOOTER_H = 30;
static const int PAD = 8;

static int rowHeight()
{
    return displayFontAscent() - displayFontDescent() + PAD;
}

static int rowsPerPage()
{
    int rows = (displayHeight() - HEADER_H - FOOTER_H) / rowHeight();
    return rows < 1 ? 1 : rows;
}

static int currentPage() { return selectedIndex / rowsPerPage(); }

static int totalPages()
{
    int r = rowsPerPage();
    return (((int)files.size() / r) + 1);
}

static void drawMenu()
{
    displayText(10, 22, "Library");

    if (files.empty())
    {
        displayText(10, HEADER_H + 30, "No books found");
        return;
    }

    int rows = rowsPerPage();
    int rowH = rowHeight();
    int first_book = currentPage() * rows;
    int maxW = displayWidth() - 2 * 14;

    for (int i = 0; i < rows && first_book + i < (int)files.size(); i++)
    {
        int idx = first_book + i;
        int top = (HEADER_H + (i * rowH));

        char buf[96];
        strncpy(buf, files[idx].title.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        int len = strlen(buf);
        while (len > 0 && displayTextWidth(buf) > maxW)
        {
            len--;
            while (len > 0 && (buf[len] & 0xC0) == 0x80)
                len--;
            buf[len] = 0;
        }

        displayText(14, top + PAD / 2 + displayFontAscent(), buf);

        if (idx == selectedIndex)
            displayRect(4, top, displayWidth() - 8, rowH);
    }

    char footer[16];
    snprintf(footer, sizeof(footer), "%d/%d", currentPage() + 1, totalPages());
    displayText(10, displayHeight() - 8, footer);
}

void menuOpen()
{
    selectedIndex = 0;
    // files = listFiles();
    extractMetadata();
    displayRender(drawMenu, true);
}

void goToNextBook()
{
    if (files.empty())
        return;
    selectedIndex = (selectedIndex + 1) % files.size();
    displayRender(drawMenu, false);
}

void goToPreviousBook()
{
    if (files.empty())
        return;
    selectedIndex = (selectedIndex - 1 + files.size()) % files.size(); // wraps to the last book
    displayRender(drawMenu, false);
}

void openBook()
{
    if (files.empty())
        return;
    Serial.println(files[selectedIndex].title);
}