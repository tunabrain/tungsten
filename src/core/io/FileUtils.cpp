#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "FileUtils.hpp"

namespace Tungsten
{

std::string FileUtils::loadText(const char *path)
{
    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);

    if (!in.good())
        return "";

    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    std::string text(size, '\0');
    in.seekg(0);
    in.read(&text[0], size);

    return std::move(text);
}

void FileUtils::changeCurrentDir(const std::string &dir)
{
    chdir(dir.c_str());
}

std::string FileUtils::getCurrentDir()
{
    char path[1024];
    getcwd(path, sizeof(path));

    return std::move(std::string(path));
}

bool FileUtils::fileExists(const std::string &path)
{
    struct stat s;
    return stat(path.c_str(), &s) == 0;
}

bool FileUtils::createDirectory(const std::string &path)
{
    std::string p(stripSlash(path));

    if (fileExists(p)) {
        return true;
    } else {
        std::string parent = extractDir(p);
        if (parent.empty() || createDirectory(parent))
            return mkdir(extractFile(p).c_str()) == 0;
        return false;
    }
}

bool FileUtils::copyFile(const std::string &src, const std::string &dst, bool createDstDir)
{
    if (createDstDir) {
        std::string parent(extractDir(dst));
        if (!parent.empty() && !createDirectory(parent))
            return false;
    }

    std::ifstream srcStream(src, std::ios::in  | std::ios::binary);

    if (srcStream.good()) {
        std::ofstream dstStream(dst, std::ios::out | std::ios::binary);
        if (dstStream.good()) {
            dstStream << srcStream.rdbuf();
            return true;
        }
    }
    return false;
}

std::string FileUtils::addSlash(std::string s)
{
    if (s.empty() || s.back() == '/')
        return std::move(s);
    s += '/';
    return std::move(s);
}

std::string FileUtils::stripSlash(std::string s)
{
    if (s.back() == '/')
        s.pop_back();

    return std::move(s);
}

std::string FileUtils::stripExt(std::string s)
{
    size_t slashPos = s.find_last_of('/');
    if (slashPos == std::string::npos)
        slashPos = 0;
    size_t dotPos = s.find_last_of('.', slashPos);

    if (dotPos != std::string::npos)
        s.erase(dotPos);

    return std::move(s);
}

std::string FileUtils::extractExt(std::string s)
{
    size_t dotPos = s.find_last_of('.');
    if (dotPos != std::string::npos)
        return s.substr(dotPos + 1);
    else
        return "";
}

std::string FileUtils::stripDir(std::string s)
{
    size_t slashPos = s.find_last_of('/');
    if (slashPos != std::string::npos)
        s.erase(0, slashPos + 1);

    return std::move(s);
}

std::string FileUtils::extractDir(std::string s)
{
    size_t slashPos = s.find_last_of('/');
    if (slashPos != std::string::npos)
        return s.substr(0, slashPos);
    else
        return "";
}

std::string FileUtils::extractFile(std::string s)
{
    return stripExt(stripDir(std::move(s)));
}

}
