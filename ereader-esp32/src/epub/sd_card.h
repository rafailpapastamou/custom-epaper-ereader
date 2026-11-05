#include <SPI.h>
#include <SD.h>
#include <vector>
#include <Arduino.h>
#include <miniz.h>
#include "zipFile.h"

#define SD_CS 15

struct file_entry
{
    String name;
};

struct Book
{
    String path;
    String title;
    String author;
    std::vector<uint8_t> cover;
};

void sdInit(SPIClass &spi);
void extractMetadata();