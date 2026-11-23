#include <string>

struct BookProgress
{
    int chapter = 0;
    int page = 0;
};

bool progressLoad(const std::string &path, BookProgress &out);
void progressSave(const std::string &path, int chapter, int page);
std::string progressLastBookPath();