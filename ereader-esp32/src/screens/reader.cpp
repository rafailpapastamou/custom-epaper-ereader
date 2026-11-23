#include "screens/reader.h"

#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "display/display.h"
#include "atomicEpub.h"
#include "epub/xmlParser.h"
#include "screens/main_menu.h"
#include "epub/progress.h"
#include "screens/settings.h"

static Epub *activeBook = nullptr;

static int chapterIndex = 0;
static int pageIndex = 0;

static std::vector<std::vector<std::string>> pages;
static std::string chapterTitle;

static bool readerReady = false;

static const int READER_HEADER_H = 0;
static const int READER_FOOTER_H = 24;
static const int READER_LEFT = 12;
static const int READER_RIGHT = 12;
static const int LINE_GAP = 3;

// --------------------------------------------------
// Display helpers
// --------------------------------------------------

static int readerLineHeight()
{
    int h = displayFontAscent() - displayFontDescent() + LINE_GAP;
    return h > 0 ? h : 16;
}

static int linesPerPage()
{
    const int top = READER_HEADER_H + 4;
    const int bottom = displayHeight() - READER_FOOTER_H;

    int n = (bottom - top) / readerLineHeight();

    return n > 0 ? n : 1;
}

static int pageCount()
{
    return pages.empty() ? 1 : (int)pages.size();
}

// --------------------------------------------------
// UTF-8 helpers
// --------------------------------------------------

static void removeLastUtf8Character(std::string &text)
{
    if (text.empty())
        return;

    size_t pos = text.size() - 1;

    while (pos > 0 &&
           ((static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80))
    {
        --pos;
    }

    text.resize(pos);
}

static std::string truncateToWidth(const std::string &text, int maxWidth)
{
    std::string result = text;

    while (!result.empty() &&
           displayTextWidth(result.c_str()) > maxWidth)
    {
        removeLastUtf8Character(result);
    }

    return result;
}

// --------------------------------------------------
// Text wrapping
// --------------------------------------------------

static void appendWrappedLine(
    std::vector<std::string> &lines,
    const std::string &line)
{
    if (!line.empty())
        lines.push_back(line);
}

static void wrapParagraph(
    const std::string &paragraph,
    std::vector<std::string> &lines,
    int maxWidth)
{
    std::string current;
    std::string word;

    auto flushWord = [&]()
    {
        if (word.empty())
            return;

        std::string candidate =
            current.empty() ? word : current + " " + word;

        if (displayTextWidth(candidate.c_str()) <= maxWidth)
        {
            current = candidate;
        }
        else
        {
            if (!current.empty())
            {
                appendWrappedLine(lines, current);
                current.clear();
            }

            // Split long words at UTF-8 character boundaries.
            std::string piece;

            for (size_t i = 0; i < word.size();)
            {
                unsigned char c =
                    static_cast<unsigned char>(word[i]);

                size_t charSize =
                    (c < 0x80) ? 1 : (c & 0xE0) == 0xC0 ? 2
                                 : (c & 0xF0) == 0xE0   ? 3
                                 : (c & 0xF8) == 0xF0   ? 4
                                                        : 1;

                if (i + charSize > word.size())
                    charSize = 1;

                std::string next =
                    piece + word.substr(i, charSize);

                if (!piece.empty() &&
                    displayTextWidth(next.c_str()) > maxWidth)
                {
                    appendWrappedLine(lines, piece);
                    piece.clear();
                }

                piece += word.substr(i, charSize);
                i += charSize;
            }

            current = piece;
        }

        word.clear();
    };

    for (char c : paragraph)
    {
        if (c == ' ' || c == '\t')
        {
            flushWord();
        }
        else
        {
            word += c;
        }
    }

    flushWord();

    if (!current.empty())
        appendWrappedLine(lines, current);
}

// --------------------------------------------------
// Pagination
// --------------------------------------------------

static void paginateText(const std::string &text)
{
    std::vector<std::string> lines;

    std::string paragraph;

    const int maxWidth =
        displayWidth() - READER_LEFT - READER_RIGHT;

    for (char c : text)
    {
        if (c == '\n')
        {
            wrapParagraph(paragraph, lines, maxWidth);
            paragraph.clear();

            // Preserve paragraph spacing.
            if (!lines.empty() && !lines.back().empty())
                lines.push_back("");
        }
        else
        {
            paragraph += c;
        }
    }

    wrapParagraph(paragraph, lines, maxWidth);

    pages.clear();

    const int capacity = linesPerPage();

    for (size_t i = 0; i < lines.size(); i += capacity)
    {
        std::vector<std::string> page;

        for (int j = 0;
             j < capacity && i + j < lines.size();
             ++j)
        {
            page.push_back(lines[i + j]);
        }

        pages.push_back(std::move(page));
    }

    // Always keep at least one page.
    if (pages.empty())
        pages.emplace_back();

    pageIndex = 0;
}

// --------------------------------------------------
// Chapter loading
// --------------------------------------------------

static bool loadChapter(int index)
{
    displaySetFont(settingsReadingFont());

    if (!activeBook ||
        index < 0 ||
        index >= activeBook->get_spine_items_count())
    {
        return false;
    }

    chapterIndex = index;

    size_t size = 0;

    uint8_t *bytes = activeBook->get_item_contents(
        activeBook->get_spine_item(chapterIndex),
        &size);

    if (!bytes || size == 0)
    {
        if (bytes)
            free(bytes);

        chapterTitle = "Unable to read chapter";

        pages.clear();
        pages.emplace_back();

        pageIndex = 0;

        return false;
    }

    // Copy the ZIP buffer before releasing it.
    // std::string::c_str() already provides null termination.
    std::string xml(
        reinterpret_cast<const char *>(bytes),
        size);

    free(bytes);

    std::string extracted;

    if (!xmlExtractText(xml, extracted))
    {
        chapterTitle = "Unsupported XHTML";

        pages.clear();
        pages.emplace_back();

        pageIndex = 0;

        return false;
    }

    // Find the corresponding TOC title.
    chapterTitle = "Chapter";

    for (int i = 0;
         i < activeBook->get_toc_items_count();
         ++i)
    {
        if (activeBook->get_spine_index_for_toc_index(i) ==
            chapterIndex)
        {
            chapterTitle =
                activeBook->get_toc_item(i).title;

            if (chapterTitle.empty())
                chapterTitle = "Chapter";

            break;
        }
    }

    paginateText(extracted);

    return true;
}

// --------------------------------------------------
// Reader rendering
// --------------------------------------------------

static void renderReaderScreen()
{
    displaySetFont(settingsReadingFont());

    if (!readerReady || !activeBook)
    {
        displayText(12, 28, "Unable to open book");
        displayText(12, 54, "Press back for library");
        return;
    }

    // Render current page.
    const int lineH = readerLineHeight();

    int y = READER_HEADER_H + 4 + displayFontAscent();

    if (pageIndex >= 0 && pageIndex < pageCount())
    {
        const std::vector<std::string> &page =
            pages[pageIndex];

        for (const std::string &line : page)
        {
            if (!line.empty())
            {
                displayText(
                    READER_LEFT,
                    y,
                    line.c_str());
            }

            y += lineH;
        }
    }

    // Footer: chapter on the left, page number on the right.
    char chapterFooter[32];
    char pageFooter[16];

    snprintf(
        chapterFooter,
        sizeof(chapterFooter),
        "Chapter %d",
        chapterIndex + 1);

    snprintf(
        pageFooter,
        sizeof(pageFooter),
        "%d/%d",
        pageIndex + 1,
        pageCount());

    // Chapter number: bottom left.
    displayText(
        READER_LEFT,
        displayHeight() - 7,
        chapterFooter);

    // Page number: bottom right.
    displayText(
        displayWidth() - READER_RIGHT -
            displayTextWidth(pageFooter),
        displayHeight() - 7,
        pageFooter);
}

// --------------------------------------------------
// Saving Progress
// --------------------------------------------------

static std::string activePath;

static void saveProgress()
{
    if (readerReady && !activePath.empty())
        progressSave(activePath, chapterIndex, pageIndex);
}

// --------------------------------------------------
// Public reader functions
// --------------------------------------------------

bool readerIsOpen()
{
    return readerReady && activeBook != nullptr;
}

void readerRender(bool fullRefresh)
{
    displayRender(renderReaderScreen, fullRefresh);
}

bool readerOpen(const std::string &epubPath)
{
    // Release any previously opened book.
    delete activeBook;
    activeBook = nullptr;

    pages.clear();

    activePath = epubPath;

    chapterIndex = 0;
    pageIndex = 0;

    chapterTitle.clear();
    readerReady = false;

    // Allocate EPUB object safely.
    activeBook = new (std::nothrow) Epub(epubPath);

    if (!activeBook)
    {
        readerRender(true);
        return false;
    }

    readerReady = activeBook->load();

    if (!readerReady ||
        activeBook->get_spine_items_count() <= 0)
    {
        readerReady = false;
        readerRender(true);
        return false;
    }

    BookProgress saved;
    progressLoad(epubPath, saved);
    if (saved.chapter < 0 || saved.chapter >= activeBook->get_spine_items_count())
        saved = BookProgress();

    if (!loadChapter(saved.chapter))
    {
        saved = BookProgress();
        if (!loadChapter(0))
        {
            readerReady = false;
            readerRender(true);
            return false;
        }
    }

    // loadChapter() resets pageIndex to 0, so restore it afterwards
    pageIndex = saved.page;
    if (pageIndex < 0)
        pageIndex = 0;
    if (pageIndex >= pageCount())
        pageIndex = pageCount() - 1;

    saveProgress(); // also marks this as the last opened book
    readerRender(true);
    return true;
}

void goToNextPage()
{
    if (!readerReady || !activeBook)
        return;

    if (pageIndex + 1 < pageCount())
    {
        ++pageIndex;
    }
    else if (chapterIndex + 1 <
             activeBook->get_spine_items_count())
    {
        loadChapter(chapterIndex + 1);
    }

    saveProgress();
    readerRender(false);
}

void goToPreviousPage()
{
    if (!readerReady || !activeBook)
        return;

    if (pageIndex > 0)
    {
        --pageIndex;
    }
    else if (chapterIndex > 0)
    {
        if (loadChapter(chapterIndex - 1))
            pageIndex = pageCount() - 1;
    }

    saveProgress();
    readerRender(false);
}

void readerNextChapter()
{
    if (!readerReady || !activeBook)
        return;

    if (chapterIndex + 1 <
        activeBook->get_spine_items_count())
    {
        loadChapter(chapterIndex + 1);
    }

    saveProgress();
    readerRender(false);
}

void readerPreviousChapter()
{
    if (!readerReady || !activeBook)
        return;

    if (chapterIndex > 0)
        loadChapter(chapterIndex - 1);

    saveProgress();
    readerRender(false);
}

void readerOpenTocEntry(int tocIndex)
{
    if (!readerReady || !activeBook)
        return;

    if (tocIndex < 0 ||
        tocIndex >= activeBook->get_toc_items_count())
    {
        return;
    }

    int target =
        activeBook->get_spine_index_for_toc_index(tocIndex);

    if (target >= 0 &&
        target < activeBook->get_spine_items_count())
    {
        loadChapter(target);
    }

    saveProgress();
    readerRender(false);
}

void readerClose()
{
    saveProgress();
    activePath.clear();

    delete activeBook;
    activeBook = nullptr;

    readerReady = false;

    chapterIndex = 0;
    pageIndex = 0;

    chapterTitle.clear();
    pages.clear();

    displayUseUiFont();
    // Return to library.
    menuOpen();
}

int readerTocCount()
{
    return activeBook ? activeBook->get_toc_items_count() : 0;
}

std::string readerTocTitle(int i)
{
    if (!activeBook || i < 0 || i >= activeBook->get_toc_items_count())
        return "";
    return activeBook->get_toc_item(i).title;
}

int readerCurrentTocIndex()
{
    if (!activeBook)
        return 0;
    for (int i = 0; i < activeBook->get_toc_items_count(); ++i)
        if (activeBook->get_spine_index_for_toc_index(i) == chapterIndex)
            return i;
    return 0;
}

void readerResume()
{
    readerRender(true);
}

void readerReflow()
{
    if (!readerReady || !activeBook)
        return;

    int oldPage = pageIndex;
    int oldCount = pageCount();

    if (loadChapter(chapterIndex))
    {
        // keep roughly the same position in the chapter
        pageIndex = (int)((long)oldPage * pageCount() / oldCount);
        if (pageIndex >= pageCount())
            pageIndex = pageCount() - 1;
    }
    saveProgress();
}