#include "epub/progress.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <map>

static const char *PROGRESS_FILE = "/sd/progress.txt";
static const char *PROGRESS_TMP = "/sd/progress.tmp";

static std::map<std::string, BookProgress> table;
static std::string lastPath;
static bool loaded = false;

// File format, one entry per line:
//   last<TAB>/sd/book.epub
//   /sd/book.epub<TAB>chapter<TAB>page
static void ensureLoaded()
{
    if (loaded)
        return;
    loaded = true;

    FILE *f = fopen(PROGRESS_FILE, "r");
    if (!f)
        return;

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();

        if (s.rfind("last\t", 0) == 0)
        {
            lastPath = s.substr(5);
            continue;
        }

        size_t b = s.rfind('\t');
        if (b == std::string::npos || b == 0)
            continue;
        size_t a = s.rfind('\t', b - 1);
        if (a == std::string::npos)
            continue;

        BookProgress p;
        p.chapter = atoi(s.c_str() + a + 1);
        p.page = atoi(s.c_str() + b + 1);
        table[s.substr(0, a)] = p;
    }
    fclose(f);
}

static void flushToSd()
{
    // Write a temp file first so a power loss can't corrupt the real one
    FILE *f = fopen(PROGRESS_TMP, "w");
    if (!f)
    {
        Serial.println("Progress: cannot write file");
        return;
    }

    fprintf(f, "last\t%s\n", lastPath.c_str());
    for (const auto &kv : table)
        fprintf(f, "%s\t%d\t%d\n", kv.first.c_str(), kv.second.chapter, kv.second.page);
    fclose(f);

    remove(PROGRESS_FILE); // FAT can't rename over an existing file
    rename(PROGRESS_TMP, PROGRESS_FILE);
}

bool progressLoad(const std::string &path, BookProgress &out)
{
    ensureLoaded();
    out = BookProgress();
    auto it = table.find(path);
    if (it == table.end())
        return false;
    out = it->second;
    return true;
}

void progressSave(const std::string &path, int chapter, int page)
{
    ensureLoaded();
    BookProgress p;
    p.chapter = chapter;
    p.page = page;
    table[path] = p;
    lastPath = path;
    flushToSd();
}

std::string progressLastBookPath()
{
    ensureLoaded();
    return lastPath;
}