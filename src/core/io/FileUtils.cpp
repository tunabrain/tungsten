#include "FileUtils.hpp"
#include "FileStreambuf.hpp"
#include "UnicodeUtils.hpp"
#include "ZipStreambuf.hpp"
#include "ZipReader.hpp"
#include "Path.hpp"

#include <tinyformat/tinyformat.hpp>
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
#include <cstdio>
#include <memory>
#include <locale>

namespace Tungsten {

static Path getNativeCurrentDir();

std::unordered_map<Path, std::shared_ptr<ZipReader>> FileUtils::_archives;
std::unordered_map<const std::ios *, FileUtils::StreamMetadata> FileUtils::_metaData;
Path FileUtils::_currentDir = getNativeCurrentDir();

typedef std::string::size_type SizeType;

#if _WIN32
static wchar_t tmpBuffer[2048];
#else
static char tmpBuffer[PATH_MAX*2];
#endif

#if _WIN32
typedef struct __stat64 NativeStatStruct;

static bool execNativeStat(const Path &p, NativeStatStruct &dst)
{
    // For whatever reason, _wstat64 does not support long paths,
    // so we omit the \\?\ prefix here
    std::string path = p.normalize().stripSeparator().nativeSeparators().asString();
    std::wstring wpath = UnicodeUtils::utf8ToWchar(path);
    return _wstat64(wpath.c_str(), &dst) == 0;
}

static std::wstring makeWideLongPath(const Path &p)
{
    std::string path = p.normalize().nativeSeparators().asString();
    return UnicodeUtils::utf8ToWchar("\\\\?\\" + path);
}
#elif __APPLE__
typedef struct stat NativeStatStruct;

static bool execNativeStat(const Path &p, NativeStatStruct &dst)
{
    return stat(p.absolute().asString().c_str(), &dst) == 0;
}

#else
typedef struct stat64 NativeStatStruct;

static bool execNativeStat(const Path &p, NativeStatStruct &dst)
{
    return stat64(p.absolute().asString().c_str(), &dst) == 0;
}
#endif

class OpenZipArchiveDir : public OpenDir
{
    std::shared_ptr<ZipReader> _archive;
    const ZipEntry &_entry;
    int _index;

public:
    OpenZipArchiveDir(std::shared_ptr<ZipReader> archive, const ZipEntry &entry)
    : _archive(archive),
      _entry(entry),
      _index(0)
    {
    }

    virtual bool increment(Path &dst, Path &parent, std::function<bool(const Path &)> acceptor) override final
    {
        if (_archive) {
            while (true) {
                if (_index == int(_entry.contents.size())) {
                    _archive.reset();
                    return false;
                }
                const ZipEntry &entry = _archive->entry(_entry.contents[_index++]);

                Path path = parent/entry.name;
                if (!acceptor(path))
                    continue;
                dst = path;

                return true;
            }
        }

        return false;
    }

    virtual bool open() const override final
    {
        return _archive.operator bool();
    }
};

class OpenFileSystemDir : public OpenDir
{
#if _WIN32
    _WDIR *_dir;
#else
    DIR *_dir;
#endif

    void close()
    {
        if (_dir)
#if _WIN32
            _wclosedir(_dir);
#else
            closedir(_dir);
#endif
        _dir = nullptr;
    }

public:
    OpenFileSystemDir(const Path &p)
#if _WIN32
    : _dir(_wopendir(makeWideLongPath(p).c_str()))
#else
    : _dir(opendir(p.absolute().asString().c_str()))
#endif
    {
    }

    ~OpenFileSystemDir()
    {
        close();
    }

    virtual bool increment(Path &dst, Path &parent, std::function<bool(const Path &)> acceptor) override final
    {
        if (_dir) {
            while (true) {
#if _WIN32
                _wdirent *entry = _wreaddir(_dir);
#else
                dirent *entry = readdir(_dir);
#endif
                if (!entry) {
                    close();
                    return false;
                }

#if _WIN32
                std::string fileName(UnicodeUtils::wcharToUtf8(entry->d_name, entry->d_namlen));
#else
#  if _DIRENT_HAVE_D_NAMLEN
                std::string fileName(entry->d_name, entry->d_namlen);
#  else
                std::string fileName(entry->d_name);
#  endif
#endif

                if (fileName == "." || fileName == "..")
                    continue;

                Path path = parent/fileName;
                if (!acceptor(path))
                    continue;
                dst = path;

                return true;
            }
        }

        return false;
    }

    virtual bool open() const override final
    {
        return _dir != nullptr;
    }
};

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

static Path getNativeCurrentDir()
{
#if _WIN32
    DWORD size = GetCurrentDirectoryW(sizeof(tmpBuffer), tmpBuffer);
    std::string result;
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<wchar_t[]> tmpBuf(new wchar_t[size + 1]);
        size = GetCurrentDirectoryW(size + 1, tmpBuf.get());
        if (size)
            result = UnicodeUtils::wcharToUtf8(tmpBuf.get(), size);
    } else if (size != 0) {
        result = UnicodeUtils::wcharToUtf8(tmpBuffer, size);
    }
    // Get rid of long path prefix if necessary
    if (result.find("\\\\?\\") == 0)
        result.erase(0, 4);
    return Path(result).normalizeSeparators();
#else
    if (getcwd(tmpBuffer, sizeof(tmpBuffer)))
        return Path(tmpBuffer);
#endif
    // Native API failed us. Not ideal.
    return Path();
}

void FileUtils::finalizeStream(std::ios *stream)
{
    auto iter = _metaData.find(stream);

    delete stream;

    if (iter != _metaData.end()) {
        iter->second.streambuf.reset();

        if (!iter->second.targetPath.empty())
            moveFile(iter->second.srcPath, iter->second.targetPath, true);
        _metaData.erase(iter);
    }
}

OutputStreamHandle FileUtils::openFileOutputStream(const Path &p)
{
#if _WIN32
    AutoFilePtr file(_wfopen(makeWideLongPath(p).c_str(), L"wb"), std::fclose);
    if (!file)
        return nullptr;

    std::unique_ptr<FileOutputStreambuf> streambuf(new FileOutputStreambuf(std::move(file)));
    std::shared_ptr<std::ostream> out(new std::ostream(streambuf.get()),
            [](std::ostream *stream){ finalizeStream(stream); });
    _metaData.insert(std::make_pair(out.get(), std::move(StreamMetadata(std::move(streambuf)))));
#else
    std::shared_ptr<std::ostream> out(new std::ofstream(p.absolute().asString(),
            std::ios_base::out | std::ios_base::binary),
            [](std::ostream *stream){ finalizeStream(stream); });

    if (!out->good())
        return nullptr;

    _metaData.insert(std::make_pair(out.get(), StreamMetadata()));
#endif

    return std::move(out);
}

std::shared_ptr<ZipReader> FileUtils::openArchive(const Path &p)
{
    Path key = p.normalize();
    auto iter = _archives.find(key);
    if (iter != _archives.end())
        return iter->second;

    std::shared_ptr<ZipReader> archive;
    try {
        archive = std::make_shared<ZipReader>(p);
    } catch (const std::runtime_error &) {
        return nullptr;
    }

    _archives.insert(std::make_pair(key, archive));
    return std::move(archive);
}

bool FileUtils::recursiveArchiveFind(const Path &p, std::shared_ptr<ZipReader> &archive,
        const ZipEntry *&entry)
{
    if (!archive) {
        Path stem = p.normalize().parent().stripSeparator();
        Path leaf = p.fileName();
        while (!stem.empty()) {
            NativeStatStruct stat;
            if (execNativeStat(stem, stat)) {
                if (S_ISREG(stat.st_mode)) {
                    archive = openArchive(stem);
                    if (archive)
                        return recursiveArchiveFind(leaf, archive, entry);
                    else
                        return false;
                } else {
                    return false;
                }
            }
            leaf = stem.fileName()/leaf;
            stem = stem.parent().stripSeparator();
        }
    } else {
        entry = archive->findEntry(p);
        if (entry)
            return true;

        Path stem = p.parent().stripSeparator();
        Path leaf = p.fileName();
        while (!stem.empty()) {
            const ZipEntry *nestedZip = archive->findEntry(stem);
            if (nestedZip) {
                if (!nestedZip->isDirectory) {
                    archive = openArchive(archive->path()/stem);
                    if (archive)
                        return recursiveArchiveFind(leaf, archive, entry);
                    else
                        return false;
                } else{
                    return false;
                }
            }
            leaf = stem.fileName()/leaf;
            stem = stem.parent().stripSeparator();
        }
    }

    return false;
}

bool FileUtils::execStat(const Path &p, StatStruct &dst)
{
    NativeStatStruct stat;
    if (execNativeStat(p, stat)) {
        dst.size        = stat.st_size;
        dst.isDirectory = S_ISDIR(stat.st_mode);
        dst.isFile      = S_ISREG(stat.st_mode);
        return true;
    }

    std::shared_ptr<ZipReader> archive;
    const ZipEntry *entry = nullptr;
    if (recursiveArchiveFind(p, archive, entry)) {
        dst.size        = entry->size;
        dst.isDirectory = entry->isDirectory;
        dst.isFile      = !entry->isDirectory;
        return true;
    }

    return false;
}

bool FileUtils::changeCurrentDir(const Path &dir)
{
    _currentDir = dir.absolute();
    return true;
}

Path FileUtils::getCurrentDir()
{
    return _currentDir;
}

Path FileUtils::getExecutablePath()
{
#if _WIN32
    DWORD size = GetModuleFileNameW(nullptr, tmpBuffer, sizeof(tmpBuffer));
    if (size >= sizeof(tmpBuffer)) {
        std::unique_ptr<wchar_t[]> tmpBuf(new wchar_t[size + 1]);
        size = GetModuleFileNameW(nullptr, tmpBuf.get(), size + 1);
        if (size)
            return Path(UnicodeUtils::wcharToUtf8(tmpBuf.get(), size)).normalizeSeparators();
    } else if (size != 0) {
        return Path(UnicodeUtils::wcharToUtf8(tmpBuffer, size)).normalizeSeparators();
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

Path FileUtils::getDataPath()
{
#if _WIN32
    Path execPath = getExecutablePath().parent()/"data";
    if (execPath.exists())
        return std::move(execPath);
    return Path(INSTALL_PREFIX)/"data";
#else
    Path execPath = getExecutablePath().parent()/"share/tungsten";
    if (execPath.exists())
        return std::move(execPath);
    return Path(INSTALL_PREFIX)/"share/tungsten";
#endif
}

uint64 FileUtils::fileSize(const Path &path)
{
    StatStruct info;
    if (!execStat(path, info))
        return 0;
    return info.size;
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
        return CreateDirectoryW(makeWideLongPath(path).c_str(), nullptr) != 0;
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
    if (size == 0 || !in || !isFile(path))
        return std::string();

    // Strip UTF-8 byte order mark if present (mostly a problem on
    // windows platforms).
    // We could also detect other byte order marks here (UTF-16/32),
    // but generally we can't deal with these encodings anyway. Best
    // to just leave it for now.
    int offset = 0;
    uint8 head[] = {0, 0, 0};
    if (in->good()) head[0] = in->get();
    if (in->good()) head[1] = in->get();
    if (in->good()) head[2] = in->get();
    if (head[0] == 0xEF && head[1] == 0xBB && head[2] == 0xBF)
        size -= 3;
    else
        offset = 3;

    std::string text(size_t(size), '\0');
    for (int i = 0; i < offset; ++i)
        text[i] = head[sizeof(head) - offset + i];
    in->read(&text[offset], size);

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
    return CopyFileW(makeWideLongPath(src).c_str(), makeWideLongPath(dst).c_str(), false) != 0;
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

bool FileUtils::moveFile(const Path &src, const Path &dst, bool deleteDst)
{
    if (dst.exists()) {
        if (!deleteDst) {
            return false;
        } else {
#if _WIN32
        return ReplaceFileW(makeWideLongPath(dst).c_str(), makeWideLongPath(src).c_str(), NULL, 0, 0, 0) != 0;
#else
        return rename(src.absolute().asString().c_str(), dst.absolute().asString().c_str()) == 0;
#endif

        }
    } else {
#if _WIN32
        return MoveFileW(makeWideLongPath(src).c_str(), makeWideLongPath(dst).c_str()) != 0;
#else
        return rename(src.absolute().asString().c_str(), dst.absolute().asString().c_str()) == 0;
#endif
    }
}

bool FileUtils::deleteFile(const Path &path)
{
#if _WIN32
    return DeleteFileW(makeWideLongPath(path).c_str()) != 0;
#else
    return std::remove(path.absolute().asString().c_str()) == 0;
#endif
}

InputStreamHandle FileUtils::openInputStream(const Path &p)
{
    NativeStatStruct info;
    if (execNativeStat(p, info)) {
#if _WIN32
        AutoFilePtr file(_wfopen(makeWideLongPath(p).c_str(), L"rb"), std::fclose);
        if (!file)
            return nullptr;

        std::unique_ptr<FileInputStreambuf> streambuf(new FileInputStreambuf(std::move(file)));
        std::shared_ptr<std::istream> in(new std::istream(streambuf.get()),
                [](std::istream *stream){ finalizeStream(stream); });
        _metaData.insert(std::make_pair(in.get(), StreamMetadata(std::move(streambuf))));
#else
        std::shared_ptr<std::istream> in(new std::ifstream(p.absolute().asString(),
                std::ios_base::in | std::ios_base::binary),
                [](std::istream *stream){ finalizeStream(stream); });

        if (!in->good())
            return nullptr;
#endif
        return std::move(in);
    }

    std::shared_ptr<ZipReader> archive;
    const ZipEntry *entry = nullptr;
    if (recursiveArchiveFind(p, archive, entry)) {
        std::unique_ptr<ZipInputStreambuf> streambuf = archive->openStreambuf(*entry);
        if (!streambuf)
            return nullptr;

        std::shared_ptr<std::istream> in(new std::istream(streambuf.get()),
                [](std::istream *stream){ finalizeStream(stream); });
        _metaData.insert(std::make_pair(in.get(), StreamMetadata(std::move(streambuf), std::move(archive))));

        return std::move(in);
    }

    return nullptr;
}

OutputStreamHandle FileUtils::openOutputStream(const Path &p)
{
    if (!p.exists())
        return openFileOutputStream(p);

    Path tmpPath(p + ".tmp");
    int index = 0;
    while (tmpPath.exists())
        tmpPath = p + tfm::format(".tmp%03d", ++index);

    OutputStreamHandle out = openFileOutputStream(tmpPath);
    if (out) {
        auto iter = _metaData.find(out.get());
        iter->second.srcPath = tmpPath;
        iter->second.targetPath = p;
    }

    return std::move(out);
}

std::shared_ptr<OpenDir> FileUtils::openDirectory(const Path &p)
{
    NativeStatStruct info;
    if (execNativeStat(p, info)) {
        if (S_ISREG(info.st_mode)) {
            std::shared_ptr<ZipReader> archive = openArchive(p);
            if (!archive)
                return nullptr;
            return std::make_shared<OpenZipArchiveDir>(std::move(archive), *archive->findEntry("."));
        } else if (S_ISDIR(info.st_mode))
            return std::make_shared<OpenFileSystemDir>(p);
        else
            return nullptr;
    }

    std::shared_ptr<ZipReader> archive;
    const ZipEntry *entry = nullptr;
    if (recursiveArchiveFind(p, archive, entry)) {
        if (entry->isDirectory) {
            return std::make_shared<OpenZipArchiveDir>(std::move(archive), *entry);
        } else {
            std::shared_ptr<ZipReader> archive = openArchive(p);
            if (!archive)
                return nullptr;
            return std::make_shared<OpenZipArchiveDir>(std::move(archive), *archive->findEntry("."));
        }
    }

    return nullptr;
}

bool FileUtils::exists(const Path &p)
{
    StatStruct info;
    return execStat(p, info);
}

bool FileUtils::isDirectory(const Path &p)
{
    StatStruct info;
    if (!execStat(p, info))
        return false;
    return info.isDirectory;
}

bool FileUtils::isFile(const Path &p)
{
    StatStruct info;
    if (!execStat(p, info))
        return false;
    return info.isFile;
}

}
