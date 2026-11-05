#include "epub/sd_card.h"

void sdInit(SPIClass &spi)
{
    if (!SD.begin(SD_CS, spi))
    {
        Serial.println("SD initialization failed!");
        return;
    }

    Serial.println("SD initialized successfully!");
}

void extractMetadata()
{
    Serial.println("Running extractMetadata()");

    const char *epubPath = "/Human Acts - Han Kang.epub";

    if (!SD.exists(epubPath))
    {
        Serial.println("EPUB file NOT found!");
        return;
    }

    Serial.println("EPUB file found!");

    if (!openZipFile(epubPath))
        return;

    Serial.println("Ready to extract EPUB metadata!");
}