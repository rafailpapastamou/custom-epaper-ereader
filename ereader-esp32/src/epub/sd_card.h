#include <SPI.h>
#include <SD.h>
#include <vector>
#include <Arduino.h>
#include <miniz.h>
#include "atomicEpub.h"

#define SD_CS 15

struct file_entry
{
    String name;
};

struct EpubMetadata
{
    std::string path;
    std::string title;
    std::string cover_image_path;
    std::string base_path;
    int spine_count = 0;
    std::vector<EpubTocEntry> toc;
};

extern std::vector<EpubMetadata> books;

void sdInit(SPIClass &spi);
bool load_epub_metadata(const std::string &path, EpubMetadata &out);
void load_all_epubs();
void print_all_books();
const EpubMetadata *last_opened_book();