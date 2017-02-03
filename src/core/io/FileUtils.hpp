#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

#include "ZipEntry.hpp"
#include "Path.hpp"

#include "IntTypes.hpp"

#include <rapidjson/document.h>
#include <unordered_map>
#include <functional>
#include <streambuf>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Tungsten {

class ZipReader;
class Path;

typedef std::shared_ptr<std::istream> InputStreamHandle;
typedef std::shared_ptr<std::ostream> OutputStreamHandle;

class OpenDir
{
protected:
    virtual ~OpenDir() {}
public:
    virtual bool increment(Path &dst, Path &parent, std::function<bool(const Path &)> acceptor) = 0;
    virtual bool open() const = 0;
};

// WARNING: Do not assume any functions operating on the file system to be thread-safe or re-entrant.
// The underlying operating system API as well as the implementation here do not make this safe.
class FileUtils
{
    FileUtils() {}

    struct StatStruct
    {
        uint64 size;
        bool isDirectory;
        bool isFile;
    };

    struct StreamMetadata
    {
        std::unique_ptr<std::basic_streambuf<char>> streambuf;
        std::shared_ptr<ZipReader> archive;
        Path srcPath, targetPath;

        StreamMetadata() = default;
        StreamMetadata(const StreamMetadata &) = delete;
        StreamMetadata(StreamMetadata &&o)
        : streambuf(std::move(o.streambuf)),
          archive(std::move(o.archive)),
          srcPath(std::move(o.srcPath)),
          targetPath(std::move(o.targetPath))
        {
        }
        StreamMetadata(std::unique_ptr<std::basic_streambuf<char>> streambuf_,
                std::shared_ptr<ZipReader> archive_ = nullptr)
        : streambuf(std::move(streambuf_)),
          archive(std::move(archive_))
        {
        }
    };

    static std::unordered_map<Path, std::shared_ptr<ZipReader>> _archives;
    static std::unordered_map<const std::ios *, StreamMetadata> _metaData;
    static Path _currentDir;

    static void finalizeStream(std::ios *stream);
    static OutputStreamHandle openFileOutputStream(const Path &p);

    static std::shared_ptr<ZipReader> openArchive(const Path &p);

    static bool recursiveArchiveFind(const Path &p, std::shared_ptr<ZipReader> &archive,
            const ZipEntry *&entry);

    static bool execStat(const Path &p, StatStruct &dst);

public:
    static bool changeCurrentDir(const Path &dir);
    static Path getCurrentDir();

    static Path getExecutablePath();
    static Path getDataPath();

    static uint64 fileSize(const Path &path);

    static bool createDirectory(const Path &path, bool recursive = true);

    static std::string loadText(const Path &path);
    static bool writeJson(const rapidjson::Document &document, const Path &p);

    static bool copyFile(const Path &src, const Path &dst, bool createDstDir);
    static bool moveFile(const Path &src, const Path &dst, bool deleteDst);
    static bool deleteFile(const Path &path);

    static InputStreamHandle openInputStream(const Path &p);
    static OutputStreamHandle openOutputStream(const Path &p);
    static std::shared_ptr<OpenDir> openDirectory(const Path &p);

    static bool exists(const Path &p);
    static bool isDirectory(const Path &p);
    static bool isFile(const Path &p);

    template<typename T>
    static inline void streamRead(std::istream &in, T &dst)
    {
        in.read(reinterpret_cast<char *>(&dst), sizeof(T));
    }

    template<typename T>
    static inline void streamRead(std::istream &in, std::vector<T> &dst)
    {
        in.read(reinterpret_cast<char *>(&dst[0]), dst.size()*sizeof(T));
    }

    template<typename T>
    static inline void streamRead(std::istream &in, T *dst, size_t numElements)
    {
        in.read(reinterpret_cast<char *>(dst), numElements*sizeof(T));
    }

    template<typename T>
    static inline void streamWrite(std::ostream &out, const T &src)
    {
        out.write(reinterpret_cast<const char *>(&src), sizeof(T));
    }

    template<typename T>
    static inline void streamWrite(std::ostream &out, const std::vector<T> &src)
    {
        out.write(reinterpret_cast<const char *>(&src[0]), src.size()*sizeof(T));
    }

    template<typename T>
    static inline void streamWrite(std::ostream &out, const T *src, size_t numElements)
    {
        out.write(reinterpret_cast<const char *>(src), numElements*sizeof(T));
    }

    template<typename T>
    static inline void streamRead(InputStreamHandle &in, T &dst)
    {
        streamRead(*in, dst);
    }

    template<typename T>
    static inline void streamRead(InputStreamHandle &in, std::vector<T> &dst)
    {
        streamRead(*in, dst);
    }

    template<typename T>
    static inline void streamRead(InputStreamHandle &in, T *dst, size_t numElements)
    {
        streamRead(*in, dst, numElements);
    }

    template<typename T>
    static inline T streamRead(InputStreamHandle &in)
    {
        T t;
        streamRead(in, t);
        return t;
    }

    template<typename T>
    static inline void streamWrite(OutputStreamHandle &out, const T &src)
    {
        streamWrite(*out, src);
    }

    template<typename T>
    static inline void streamWrite(OutputStreamHandle &out, const std::vector<T> &src)
    {
        streamWrite(*out, src);
    }

    template<typename T>
    static inline void streamWrite(OutputStreamHandle &out, const T *src, size_t numElements)
    {
        streamWrite(*out, src, numElements);
    }
};

template<>
inline std::string FileUtils::streamRead<std::string>(InputStreamHandle &in)
{
    std::string s;
    std::getline(*in, s, '\0');
    return std::move(s);
}

template<>
inline void FileUtils::streamWrite<std::string>(OutputStreamHandle &out, const std::string &src)
{
    streamWrite(out, &src[0], src.size() + 1);
}

}

#endif /* FILEUTILS_HPP_ */
