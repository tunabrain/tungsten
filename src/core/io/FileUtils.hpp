#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

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

    static void finalizeStream(std::ios *stream);

    static std::unordered_map<const std::ios *, std::unique_ptr<std::basic_streambuf<char>>> _streambufs;

public:
    static bool changeCurrentDir(const Path &dir);
    static Path getCurrentDir();

    static Path getExecutablePath();

    static uint64 fileSize(const Path &path);

    static bool createDirectory(const Path &path, bool recursive = true);

    static std::string loadText(const Path &path);
    static bool writeJson(const rapidjson::Document &document, const Path &p);

    static bool copyFile(const Path &src, const Path &dst, bool createDstDir);

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

}

#endif /* FILEUTILS_HPP_ */
