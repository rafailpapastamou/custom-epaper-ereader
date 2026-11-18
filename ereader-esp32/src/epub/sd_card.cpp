#include "epub/sd_card.h"
#include <iostream>
#include <filesystem>
#include <string>

void sdInit(SPIClass &spi)
{
    if (!SD.begin(SD_CS, spi))
    {
        Serial.println("SD initialization failed!");
        return;
    }

    Serial.println("SD initialized successfully!");
}

bool load_epub_metadata(const std::string &path, EpubMetadata &out)
{
    Epub epub(path);
    if (!epub.load())
    {
        return false;
    }

    out.path = epub.get_path();
    out.title = epub.get_title();
    out.cover_image_path = epub.get_cover_image_item();
    out.base_path = epub.get_base_path();
    out.spine_count = epub.get_spine_items_count();

    for (int i = 0; i < epub.get_toc_items_count(); i++)
    {
        out.toc.push_back(epub.get_toc_item(i));
    }
    return true;
}

void epubNamesToList()
{
}

std::vector<EpubMetadata> books;

void load_all_epubs()
{
    books.clear();

    for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path("/sd/")))
    {
        if (!entry.is_regular_file())
            continue;

        std::string name = entry.path().filename().string();

        if (name.empty() || name[0] == '.')
            continue;
        if (entry.path().extension() != ".epub")
            continue;

        EpubMetadata meta;

        if (!load_epub_metadata(entry.path().string(), meta))
        {
            Serial.printf("FAILED: %s\n", name.c_str());
        }
        else
        {
            books.push_back(meta);
        }
    }
}

void print_all_books()
{
    std::sort(books.begin(), books.end(),
              [](const EpubMetadata &a, const EpubMetadata &b)
              {
                  auto getSortTitle = [](const std::string &title)
                  {
                      if (title.size() > 4 &&
                          strncasecmp(title.c_str(), "The ", 4) == 0)
                          return title.c_str() + 4;

                      if (title.size() > 2 &&
                          strncasecmp(title.c_str(), "A ", 2) == 0)
                          return title.c_str() + 2;

                      if (title.size() > 3 &&
                          strncasecmp(title.c_str(), "An ", 3) == 0)
                          return title.c_str() + 3;

                      return title.c_str();
                  };

                  return strcasecmp(
                             getSortTitle(a.title),
                             getSortTitle(b.title)) < 0;
              });

    int i = 1;
    for (const auto &book : books)
    {
        Serial.printf("Book:%d: %s\n", i, book.title.c_str());
        i++;
    }
}