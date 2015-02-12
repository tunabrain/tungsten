#include "FileUtils.hpp"
#include "Path.hpp"

#include "Debug.hpp"

#include <rapidjson/prettywriter.h>
#ifdef _MSC_VER
#include <dirent/dirent.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif
#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#endif

#include <fstream>
#include <cstring>
#include <memory>
#include <locale>

namespace Tungsten {

typedef std::string::size_type SizeType;

#if _WIN32
static char tmpBuffer[2048];
#else
static char tmpBuffer[PATH_MAX*2];
#endif

class JsonOstreamWriter {
    OutputStreamHandle _out;
public:
    typedef char Ch;    //!< Character type. Only support char.

    JsonOstreamWriter(OutputStreamHandle out)
    : _out(std::move(out))
    {
    }
    void Put(char c) {
        _out->put(c);
    }
    void PutN(char c, size_t n) {
        for (size_t i = 0; i < n; ++i)
            _out->put(c);
    }
    void Flush() {
        _out->flush();
    }
};

void FileUtils::finalizeStream(std::ios *stream)
{
    delete stream;
}

bool FileUtils::changeCurrentDir(const Path &dir)
{
#if _WIN32
    return SetCurrentDirectoryA(dir.absolute().asString().c_str()) != 0;
#else
    return chdir(dir.absolute().asString().c_str()) == 0;
#endif
}

Path FileUtils::getCurrentDir()
{
#if _WIN32
    DWORD size = GetCurrentDirectory(sizeof(tmpBuffer), tmpBuffer);
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetCurrentDirectory(size + 1, tmpBuf.get());
        if (size)
            return Path(std::string(tmpBuf.get(), size)).normalizeSeparators();
    } else if (size != 0) {
        return Path(std::string(tmpBuffer, size)).normalizeSeparators();
    }
#else
    if (getcwd(tmpBuffer, sizeof(tmpBuffer)))
        return Path(tmpBuffer);
#endif
    // Native API failed us. Not ideal.
    return Path();
}

Path FileUtils::getExecutablePath()
{
#if _WIN32
    DWORD size = GetModuleFileNameA(nullptr, tmpBuffer, sizeof(tmpBuffer));
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<char[]> tmpBuf(new char[size + 1]);
        size = GetModuleFileNameA(nullptr, tmpBuf.get(), size + 1);
        if (size)
            return Path(std::string(tmpBuf.get(), size)).normalizeSeparators();
    } else if (size != 0) {
        return Path(std::string(tmpBuffer, size)).normalizeSeparators();
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

uint64 FileUtils::fileSize(const Path &path)
{
#ifdef _WIN32
    struct __stat64 info;
    if (_stat64(path.absolute().asString().c_str(), &info) != 0 || !S_ISREG(info.st_mode))
        return 0;
    return info.st_size;
#else
    struct stat64 info;
    if (stat64(path.absolute().asString().c_str(), &info) != 0 || !S_ISREG(info.st_mode))
        return 0;
    return info.st_size;
#endif
}


bool FileUtils::createDirectory(const Path &path, bool recursive)
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

std::string FileUtils::loadText(const Path &path)
{
    uint64 size = fileSize(path);
    InputStreamHandle in = openInputStream(path);
    if (size == 0 || !in)
        FAIL("Unable to open file at '%s'", path);

    std::string text(size, '\0');
    in->read(&text[0], size);

    return std::move(text);
}

bool FileUtils::writeJson(const rapidjson::Document &document, const Path &p)
{
    OutputStreamHandle stream = openOutputStream(p);
    if (!stream)
        return false;

    JsonOstreamWriter out(stream);
    rapidjson::PrettyWriter<JsonOstreamWriter> writer(out);
    document.Accept(writer);

    return true;
}

bool FileUtils::copyFile(const Path &src, const Path &dst, bool createDstDir)
{
    if (createDstDir) {
        Path parent(dst.parent());
        if (!parent.empty() && !createDirectory(parent))
            return false;
    }

#if _WIN32
    return CopyFile(src.absolute().asString().c_str(), dst.absolute().asString().c_str(), false) != 0;
#else
    InputStreamHandle srcStream = openInputStream(src);
    if (!srcStream) {
        OutputStreamHandle dstStream = openOutputStream(dst);
        if (dstStream) {
            *dstStream << srcStream->rdbuf();
            return true;
        }
    }
    return false;
#endif
}

InputStreamHandle FileUtils::openInputStream(const Path &p)
{
    std::shared_ptr<std::istream> in(new std::ifstream(p.absolute().asString(),
            std::ios_base::in | std::ios_base::binary),
            [](std::istream *stream){ finalizeStream(stream); });

    if (!in->good())
        return nullptr;

    return std::move(in);
}

OutputStreamHandle FileUtils::openOutputStream(const Path &p)
{
    std::shared_ptr<std::ostream> out(new std::ofstream(p.absolute().asString(),
            std::ios_base::out | std::ios_base::binary),
            [](std::ostream *stream){ finalizeStream(stream); });

    if (!out->good())
        return nullptr;

    return std::move(out);
}

}
