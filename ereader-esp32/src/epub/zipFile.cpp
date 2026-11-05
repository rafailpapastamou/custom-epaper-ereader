#include "zipFile.h"

static mz_zip_archive zip;
static File zipFile;
static bool zipOpened = false;

static size_t sdZipRead(
    void *opaque,
    mz_uint64 fileOffset,
    void *buffer,
    size_t size)
{
    File *file = (File *)opaque;

    if (!file || !(*file))
        return 0;

    if (!file->seek((uint32_t)fileOffset))
        return 0;

    return file->read((uint8_t *)buffer, size);
}

bool openZipFile(String path)
{
    if (zipOpened)
    {
        mz_zip_reader_end(&zip);
        zipOpened = false;
    }

    if (zipFile)
        zipFile.close();

    zipFile = SD.open(path.c_str(), FILE_READ);

    if (!zipFile)
    {
        Serial.println("Failed to open EPUB from SD!");
        return false;
    }

    mz_zip_zero_struct(&zip);

    zip.m_pRead = sdZipRead;
    zip.m_pIO_opaque = &zipFile;

    if (!mz_zip_reader_init(&zip, zipFile.size(), 0))
    {
        Serial.printf(
            "Miniz error: %s\n",
            mz_zip_get_error_string(mz_zip_get_last_error(&zip)));

        zipFile.close();
        return false;
    }

    zipOpened = true;

    Serial.println("ZIP file opened successfully!");

    Serial.printf(
        "Files inside EPUB: %u\n",
        (unsigned)mz_zip_reader_get_num_files(&zip));

    return true;
}