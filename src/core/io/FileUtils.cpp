#include "FileUtils.hpp"
#include "Debug.hpp"

#if _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#endif

#include <iostream>
#include <cstring>
#include <memory>

namespace Tungsten {

namespace FileUtils {

typedef std::string::size_type SizeType;

#if _WIN32
static char tmpBuffer[2048];
#else
static char tmpBuffer[PATH_MAX*2];
#endif

#if _WIN32
static const char *separators = "/\\";
#else
static const char *separators = "/";
#endif

static bool isSeparator(char p)
{
#if _WIN32
    return p == '/' || p == '\\';
#else
    return p == '/';
#endif
}

bool isRootDirectory(const std::string &s)
{
    if (s.size() == 1 && isSeparator(s[0]))
        return true;

#if _WIN32
    // Fully qualified drive paths (e.g. C:\)
    if (s.size() == 2 && s[1] == ':')
        return true;
    if (s.size() == 3 && s[1] == ':' && isSeparator(s[2]))
        return true;

    // UNC style path (e.g. network drives)
    if (s.size() == 2 && isSeparator(s[0]) && isSeparator(s[1]))
        return true;
#endif
    return false;
}

bool isRelativePath(const std::string &path)
{
    if (path.empty())
        return true;
    if (isSeparator(path[0]))
        return false;

#if _WIN32
    // ShlWapi functions do not play well with forward slashes.
    // Do not replace this with PathIsRelative!

    if (path.size() >= 2) {
        // UNC path (e.g. \\foo\bar.txt)
        if (isSeparator(path[0]) && isSeparator(path[1]))
            return false;
        // We're ignoring drive relative paths (e.g. C:foo.txt) here,
        // since not even the WinAPI gets them right.
        if (path[1] == ':')
            return false;
    }
#endif
    return true;
}

std::string addSeparator(std::string s)
{
    if (s.empty() || isSeparator(s.back()))
        return std::move(s);
    s += '/';
    return std::move(s);
}

std::string stripSeparator(std::string s)
{
    while (!s.empty() && isSeparator(s.back()))
        s.pop_back();

    return std::move(s);
}

static SizeType findFilenamePos(const std::string &s)
{
    if (s.empty() || isSeparator(s.back()))
        return std::string::npos;
#if _WIN32
    if (s.back() == ':')
        return std::string::npos;
#endif

    SizeType n = s.find_last_of(separators);
#if _WIN32
    if (n == std::string::npos)
        n = s.find_last_of(':');
#endif

    if (n == std::string::npos)
        return 0;
    else
        return n + 1;
}

static SizeType findExtensionPos(const std::string &s)
{
    // ShlWapi functions do not play well with forward slashes.
    // Do not replace this with PathFindExtension on Windows!

    if (s.back() == '.')
        return std::string::npos;

    SizeType start = findFilenamePos(s);
    if (start == std::string::npos)
        return std::string::npos;

    SizeType n = s.find_last_of('.');
    if (n == std::string::npos || n < start)
        return std::string::npos;
    return n + 1;
}

std::string stripExt(std::string s)
{
    SizeType extPos = findExtensionPos(s);
    if (extPos != std::string::npos)
        s.erase(extPos - 1);

    return std::move(s);
}

std::string stripParent(std::string s)
{
    if (isRootDirectory(s))
        return std::string();

    s = stripSeparator(s);

#if _WIN32
    // ShlWapi functions do not play well with forward slashes.
    // Do not replace this with PathStripPath!

    SizeType pos = findFilenamePos(s);
    if (pos == std::string::npos)
        return std::string();
    s.erase(0, pos);
    return std::move(s);
#else
    char *tmpBuf = tmpBuffer;
    std::unique_ptr<char[]> auxBuf;
    if (s.size() + 1 > sizeof(tmpBuffer)) {
        auxBuf.reset(new char[s.size() + 1]);
        tmpBuf = auxBuf.get();
    }
    std::memcpy(tmpBuf, s.c_str(), s.size() + 1);

    char *result = basename(tmpBuf);
    return std::string(result);
#endif
}

std::string extractExt(std::string s)
{
    SizeType extPos = findExtensionPos(s);
    if (extPos != std::string::npos)
        return s.substr(extPos);
    else
        return std::string();
}

std::string extractParent(std::string s)
{
    if (isRootDirectory(s))
        return std::move(s);

    s = stripSeparator(s);

#if _WIN32
    // ShlWapi functions do not play well with forward slashes.
    // Do not replace this with PathRemoveFileSpec!

    SizeType pos = findFilenamePos(s);
    if (pos == std::string::npos)
        return std::move(s);
    s.erase(pos);
    if (!isRootDirectory(s))
        s = stripSeparator(std::move(s));
    return std::move(s);
#else
    char *tmpBuf = tmpBuffer;
    std::unique_ptr<char[]> auxBuf;
    if (s.size() + 1 > sizeof(tmpBuffer)) {
        auxBuf.reset(new char[s.size() + 1]);
        tmpBuf = auxBuf.get();
    }
    std::memcpy(tmpBuf, s.c_str(), s.size() + 1);

    char *resultBuf = dirname(tmpBuf);
    std::string result(resultBuf);

    // To be consistent with Windows, we reduce naked file names to the empty string, not "."
    if (!result.empty() && result.back() == '.')
        result.pop_back();

    return std::move(result);
#endif
}

std::string extractBase(std::string s)
{
    return stripExt(stripParent(std::move(s)));
}

std::string toAbsolutePath(const std::string &path)
{
#if _WIN32
    DWORD size = GetFullPathNameA(path.c_str(), sizeof(tmpBuffer), tmpBuffer, nullptr);
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetFullPathNameA(path.c_str(), size + 1, tmpBuf.get(), nullptr);
        if (size)
            return std::string(tmpBuf.get(), size);
    } else if (size != 0) {
        return std::string(tmpBuffer, size);
    }
#else
    // MinGW does not implement realpath.
#ifndef __MINGW32__
    char *resultBuf = realpath(path.c_str(), tmpBuffer);
    if (resultBuf)
        return std::string(resultBuf);
#endif
#endif
    // Native APIs failed us, so let's do the naive solution
    if (isRelativePath(path))
        return getCurrentDir() + path;
    else
        return path;
}

bool changeCurrentDir(const std::string &dir)
{
#if _WIN32
    return SetCurrentDirectoryA(dir.c_str()) != 0;
#else
    return chdir(dir.c_str()) == 0;
#endif
}

std::string getCurrentDir()
{
#if _WIN32
    DWORD size = GetCurrentDirectory(sizeof(tmpBuffer), tmpBuffer);
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetCurrentDirectory(size + 1, tmpBuf.get());
        if (size)
            return addSeparator(std::string(tmpBuf.get(), size));
    } else if (size != 0) {
        return addSeparator(std::string(tmpBuffer, size));
    }
#else
    if (getcwd(tmpBuffer, sizeof(tmpBuffer)))
        return addSeparator(std::string(tmpBuffer));
#endif
    // Native API failed us. Not ideal.
    return std::string();
}

bool fileExists(const std::string &path)
{
#if _WIN32
    return GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat s;
    return stat(path.c_str(), &s) == 0;
#endif
}

bool createDirectory(const std::string &path, bool recursive)
{
    std::string p(stripSeparator(path));

    if (fileExists(p)) {
        return true;
    } else {
        std::string parent = extractParent(p);
        if (!parent.empty() && !fileExists(parent) && (!recursive || !createDirectory(parent)))
            return false;
#if _WIN32
        return CreateDirectory(p.c_str(), nullptr) != 0;
#else
#ifdef __MINGW32__
        // MinGW implements mkdir, but removes the second parameter (???)
        return mkdir(p.c_str()) == 0;
#else
        return mkdir(p.c_str(), 0777) == 0;
#endif
#endif
    }
}

std::string loadText(const char *path)
{
    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);

    if (!in.good())
        FAIL("Unable to open file at '%s'", path);

    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    std::string text(size, '\0');
    in.seekg(0);
    in.read(&text[0], size);

    return std::move(text);
}

bool copyFile(const std::string &src, const std::string &dst, bool createDstDir)
{
    if (createDstDir) {
        std::string parent(extractParent(dst));
        if (!parent.empty() && !createDirectory(parent))
            return false;
    }

#if _WIN32
    return CopyFile(src.c_str(), dst.c_str(), false) != 0;
#else
    std::ifstream srcStream(src, std::ios::in  | std::ios::binary);

    if (srcStream.good()) {
        std::ofstream dstStream(dst, std::ios::out | std::ios::binary);
        if (dstStream.good()) {
            dstStream << srcStream.rdbuf();
            return true;
        }
    }
    return false;
#endif
}

}

}
