#include "FileUtils.hpp"
#include "Path.hpp"

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
#include <locale>

namespace Tungsten {

namespace FileUtils {

typedef std::string::size_type SizeType;

#if _WIN32
static char tmpBuffer[2048];
#else
static char tmpBuffer[PATH_MAX*2];
#endif

bool changeCurrentDir(const Path &dir)
{
#if _WIN32
    return SetCurrentDirectoryA(dir.absolute().asString().c_str()) != 0;
#else
    return chdir(dir.absolute().asString().c_str()) == 0;
#endif
}

Path getCurrentDir()
{
#if _WIN32
    DWORD size = GetCurrentDirectory(sizeof(tmpBuffer), tmpBuffer);
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetCurrentDirectory(size + 1, tmpBuf.get());
        if (size)
            return Path(std::string(tmpBuf.get(), size));
    } else if (size != 0) {
        return Path(std::string(tmpBuffer, size));
    }
#else
    if (getcwd(tmpBuffer, sizeof(tmpBuffer)))
        return Path(tmpBuffer);
#endif
    // Native API failed us. Not ideal.
    return Path();
}

Path getExecutablePath()
{
#if _WIN32
    DWORD size = GetModuleFileNameA(nullptr, tmpBuffer, sizeof(tmpBuffer));
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetModuleFileNameA(nullptr, tmpBuf.get(), size + 1);
        if (size)
            return Path(std::string(tmpBuf.get(), size));
    } else if (size != 0) {
        return Path(std::string(tmpBuffer, size));
    }
#else
    ssize_t size = readlink("/proc/self/exe", tmpBuffer, sizeof(tmpBuffer));
    if (size != -1)
        return Path(std::string(tmpBuffer, size));
    // readlink does not tell us the actual content size if our buffer is too small,
    // so we won't attempt to allocate a larger buffer here and fail instead
#endif
    return Path();
}


bool createDirectory(const Path &path, bool recursive)
{
    if (path.exists()) {
        return true;
    } else {
        Path parent = path.parent();
        if (!parent.empty() && !parent.exists() && (!recursive || !createDirectory(parent)))
            return false;
#if _WIN32
        return CreateDirectory(path.absolute().asString().c_str(), nullptr) != 0;
#else
#ifdef __MINGW32__
        // MinGW implements mkdir, but removes the second parameter (???)
        return mkdir(path.absolute().asString().c_str()) == 0;
#else
        return mkdir(path.absolute().asString().c_str(), 0777) == 0;
#endif
#endif
    }
}

std::string loadText(const Path &path)
{
    std::ifstream in(path.absolute().asString(), std::ios_base::in | std::ios_base::binary);

    if (!in.good())
        FAIL("Unable to open file at '%s'", path);

    in.seekg(0, std::ios::end);
    size_t size = size_t(in.tellg());
    std::string text(size, '\0');
    in.seekg(0);
    in.read(&text[0], size);

    return std::move(text);
}

bool copyFile(const Path &src, const Path &dst, bool createDstDir)
{
    if (createDstDir) {
        Path parent(dst.parent());
        if (!parent.empty() && !createDirectory(parent))
            return false;
    }

#if _WIN32
    return CopyFile(src.absolute().asString().c_str(), dst.absolute().asString().c_str(), false) != 0;
#else
    std::ifstream srcStream(src.absolute().asString(), std::ios::in  | std::ios::binary);

    if (srcStream.good()) {
        std::ofstream dstStream(dst.absolute().asString(), std::ios::out | std::ios::binary);
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
