#include "screens/main_menu.h"
#include "epub/sd_card.h"
#include "display/display.h"
#include "screens/reader.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>

static int selectedIndex = 0;          // index into `filtered`
static int selectedLetterIndex = 0;    // index into `activeLetters`
static int letterScroll = 0;           // first active letter shown in the bar
static std::vector<int> filtered;      // indices into `books` for the selected letter
static std::vector<int> activeLetters; // ALL_LETTERS first, then letter-bar indices (0..50) that have books

static const int HEADER_H = 30;
static const int FOOTER_H = 30;
static const int PAD = 8;

// 0 = '#', 1..26 = A-Z, 27..50 = Α-Ω (24 Greek letters)
static const int LETTER_COUNT = 51;

// Special bar entry: blank slot that shows every book
static const int ALL_LETTERS = -1;
static const int BLANK_W = 12; // width of the blank slot in pixels

static int letterCodepoint(int i)
{
    if (i == 0)
        return '#';
    if (i <= 26)
        return 'A' + (i - 1);
    int g = i - 27;
    return 0x391 + g + (g >= 17 ? 1 : 0); // skip unassigned U+03A2
}

static int encodeUtf8(int cp, char *out)
{
    if (cp < 0x80)
    {
        out[0] = cp;
        out[1] = 0;
        return 1;
    }
    out[0] = 0xC0 | (cp >> 6);
    out[1] = 0x80 | (cp & 0x3F);
    out[2] = 0;
    return 2;
}

// Fills buf with the label for a bar entry and returns its pixel width
static int letterLabel(int letter, char *buf)
{
    if (letter == ALL_LETTERS)
    {
        buf[0] = 0;
        return BLANK_W;
    }
    encodeUtf8(letterCodepoint(letter), buf);
    return displayTextWidth(buf);
}

// Decodes one UTF-8 character at s, sets len to its byte length
static int decodeAt(const char *s, int &len)
{
    unsigned char c = s[0];
    if (c < 0x80)
    {
        len = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && s[1])
    {
        len = 2;
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2])
    {
        len = 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3])
    {
        len = 4;
        return 0xFFFF;
    }
    len = 1;
    return 0xFFFF;
}

// Uppercases Latin, uppercases Greek and strips accents / final sigma
static int foldCodepoint(int cp)
{
    if (cp >= 'a' && cp <= 'z')
        return cp - 32;

    switch (cp)
    {
    case 0x386:
    case 0x3AC:
        return 0x391; // Ά ά
    case 0x388:
    case 0x3AD:
        return 0x395; // Έ έ
    case 0x389:
    case 0x3AE:
        return 0x397; // Ή ή
    case 0x38A:
    case 0x3AF:
    case 0x3CA:
    case 0x390:
        return 0x399; // Ί ί ϊ ΐ
    case 0x38C:
    case 0x3CC:
        return 0x39F; // Ό ό
    case 0x38E:
    case 0x3CD:
    case 0x3CB:
    case 0x3B0:
        return 0x3A5; // Ύ ύ ϋ ΰ
    case 0x38F:
    case 0x3CE:
        return 0x3A9; // Ώ ώ
    case 0x3C2:
        return 0x3A3; // ς -> Σ
    }
    if (cp >= 0x3B1 && cp <= 0x3C9) // lowercase Greek -> uppercase
        return cp - 0x20;
    return cp;
}

// Articles to ignore, stored folded (uppercase, no accents), 0-terminated
static const int ARTICLES[][5] = {
    // English: THE, A, AN
    {'T', 'H', 'E', 0},
    {'A', 0},
    {'A', 'N', 0},
    // Greek: Ο Η ΤΟ ΟΙ ΤΑ
    {0x39F, 0},
    {0x397, 0},
    {0x3A4, 0x39F, 0},
    {0x39F, 0x399, 0},
    {0x3A4, 0x391, 0},
    // ΤΟΝ ΤΗΝ ΤΗΣ ΤΟΥ ΤΟΥΣ ΤΙΣ ΤΩΝ
    {0x3A4, 0x39F, 0x39D, 0},
    {0x3A4, 0x397, 0x39D, 0},
    {0x3A4, 0x397, 0x3A3, 0},
    {0x3A4, 0x39F, 0x3A5, 0},
    {0x3A4, 0x39F, 0x3A5, 0x3A3, 0},
    {0x3A4, 0x399, 0x3A3, 0},
    {0x3A4, 0x3A9, 0x39D, 0},
    // ΕΝΑΣ ΕΝΑ ΜΙΑ ΜΙΑΣ
    {0x395, 0x39D, 0x391, 0x3A3, 0},
    {0x395, 0x39D, 0x391, 0},
    {0x39C, 0x399, 0x391, 0},
    {0x39C, 0x399, 0x391, 0x3A3, 0},
};

// Returns a pointer to the start of the title without a leading article.
// Only skips when another word follows the article.
static const char *skipArticle(const std::string &title)
{
    const char *start = title.c_str();
    while (*start == ' ')
        start++;

    const char *p = start;
    int w[5];
    int n = 0;
    while (*p && *p != ' ')
    {
        int l;
        int cp = foldCodepoint(decodeAt(p, l));
        if (n < 5)
            w[n] = cp;
        n++;
        p += l;
    }

    if (*p != ' ' || n > 4)
        return start;

    const char *rest = p;
    while (*rest == ' ')
        rest++;
    if (!*rest)
        return start; // the title is only the article

    for (const auto &a : ARTICLES)
    {
        int k = 0;
        while (k < n && a[k] == w[k])
            k++;
        if (k == n && a[n] == 0)
            return rest;
    }
    return start;
}

// Returns the letter-bar index (0..50) for a title
static int letterIndexOf(const std::string &title)
{
    int l;
    int cp = foldCodepoint(decodeAt(skipArticle(title), l));

    if (cp >= 'A' && cp <= 'Z')
        return cp - 'A' + 1;

    if (cp >= 0x391 && cp <= 0x3A9 && cp != 0x3A2)
        return 27 + (cp - 0x391) - (cp > 0x3A2 ? 1 : 0);

    return 0; // digits, symbols, anything else -> '#'
}

// Sort order: ignores articles, case and Greek accents
static bool titleLess(int a, int b)
{
    const char *x = skipArticle(books[a].title);
    const char *y = skipArticle(books[b].title);
    while (*x && *y)
    {
        int lx, ly;
        int cx = foldCodepoint(decodeAt(x, lx));
        int cy = foldCodepoint(decodeAt(y, ly));
        if (cx != cy)
            return cx < cy;
        x += lx;
        y += ly;
    }
    return *x == 0 && *y != 0;
}

static void rebuildActiveLetters()
{
    bool present[LETTER_COUNT] = {false};
    for (auto &b : books)
        present[letterIndexOf(b.title)] = true;

    activeLetters.clear();
    activeLetters.push_back(ALL_LETTERS); // blank slot, always first
    for (int i = 0; i < LETTER_COUNT; i++)
        if (present[i])
            activeLetters.push_back(i);
}

static void rebuildFilter()
{
    filtered.clear();
    selectedIndex = 0;
    if (activeLetters.empty())
        return;

    int letter = activeLetters[selectedLetterIndex];
    for (int i = 0; i < (int)books.size(); i++)
        if (letter == ALL_LETTERS || letterIndexOf(books[i].title) == letter)
            filtered.push_back(i);

    std::stable_sort(filtered.begin(), filtered.end(), titleLess);
}

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
    int p = ((int)filtered.size() + r - 1) / r;
    return p < 1 ? 1 : p;
}

static void drawLetterBar()
{
    if (activeLetters.empty())
        return;

    const int gap = 10;
    char buf[4];

    // keep the selected letter visible by scrolling the bar
    if (selectedLetterIndex < letterScroll)
        letterScroll = selectedLetterIndex;
    while (true)
    {
        int x = 10;
        for (int i = letterScroll; i <= selectedLetterIndex; i++)
            x += letterLabel(activeLetters[i], buf) + gap;
        if (x <= displayWidth() || letterScroll >= selectedLetterIndex)
            break;
        letterScroll++;
    }

    int x = 10;
    for (int i = letterScroll; i < (int)activeLetters.size(); i++)
    {
        int w = letterLabel(activeLetters[i], buf);
        if (x + w > displayWidth())
            break;
        if (buf[0])
            displayText(x, 22, buf);
        if (i == selectedLetterIndex)
            displayRect(x - 3, 2, w + 6, HEADER_H - 4);
        x += w + gap;
    }
}

static size_t builtForCount = (size_t)-1; // size of `books` when the lists were last built

static void refreshIfBooksChanged()
{
    if (books.size() == builtForCount)
        return;
    builtForCount = books.size();

    rebuildActiveLetters();
    selectedLetterIndex = 0;
    letterScroll = 0;
    rebuildFilter();
}

static void drawMenu()
{
    refreshIfBooksChanged();
    drawLetterBar();

    if (filtered.empty())
    {
        displayText(10, HEADER_H + 30, "No books found");
        return;
    }

    int rows = rowsPerPage();
    int rowH = rowHeight();
    int first_book = currentPage() * rows;
    int maxW = displayWidth() - 2 * 14;

    for (int i = 0; i < rows && first_book + i < (int)filtered.size(); i++)
    {
        int idx = first_book + i;
        int top = HEADER_H + (i * rowH);
        const std::string &title = books[filtered[idx]].title;

        char buf[96];
        strncpy(buf, title.c_str(), sizeof(buf) - 1);
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

static bool first_boot = true;

void menuOpen()
{
    if (first_boot)
    {
        load_all_epubs();
        first_boot = false;
    }

    builtForCount = (size_t)-1;
    refreshIfBooksChanged();

    print_all_books();
    displayRender(drawMenu, true);
}

void goToNextBook()
{
    if (filtered.empty())
        return;
    selectedIndex = (selectedIndex + 1) % filtered.size();
    displayRender(drawMenu, false);
}

void goToPreviousBook()
{
    if (filtered.empty())
        return;
    selectedIndex = (selectedIndex - 1 + filtered.size()) % filtered.size();
    displayRender(drawMenu, false);
}

void openBook()
{
    if (filtered.empty())
        return;
    readerOpen(books[filtered[selectedIndex]].path);
}

void goToNextLetter()
{
    if (activeLetters.empty())
        return;
    selectedLetterIndex = (selectedLetterIndex + 1) % activeLetters.size();
    rebuildFilter();
    displayRender(drawMenu, false);
}

void goToPreviousLetter()
{
    if (activeLetters.empty())
        return;
    selectedLetterIndex = (selectedLetterIndex - 1 + activeLetters.size()) % activeLetters.size();
    rebuildFilter();
    displayRender(drawMenu, false);
}