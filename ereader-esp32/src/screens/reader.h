#pragma once

#include <string>

// Opens an EPUB from its path and displays the first page.
bool readerOpen(const std::string &epubPath);

// Reader navigation. Bind these to the device's existing button handlers.
void readerNextPage();
void readerPreviousPage();
void readerNextChapter();
void readerPreviousChapter();
void readerOpenTocEntry(int tocIndex);

// Return to the library screen.
void readerClose();

// True while a book is open; useful for routing hardware button events.
bool readerIsOpen();

// Redraw the current reader screen.
void readerRender(bool fullRefresh = false);
bool readerOpen(const std::string &epubPath);
void goToPreviousPage();
void goToNextPage();