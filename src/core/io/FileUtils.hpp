#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

#include "IntTypes.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace Tungsten {

class Path;

// WARNING: Do not assume any functions operating on the file system to be thread-safe or re-entrant.
// The underlying operating system API as well as the implementation here do not make this safe.
namespace FileUtils {

bool changeCurrentDir(const Path &dir);
Path getCurrentDir();

Path getExecutablePath();

bool createDirectory(const Path &path, bool recursive = true);

std::string loadText(const Path &path);

bool copyFile(const Path &src, const Path &dst, bool createDstDir);

template<typename T>
inline void streamRead(std::istream &in, T &dst)
{
    in.read(reinterpret_cast<char *>(&dst), sizeof(T));
}

template<typename T>
inline void streamRead(std::istream &in, std::vector<T> &dst)
{
    in.read(reinterpret_cast<char *>(&dst[0]), dst.size()*sizeof(T));
}

template<typename T>
inline void streamRead(std::istream &in, T *dst, size_t numElements)
{
    in.read(reinterpret_cast<char *>(dst), numElements*sizeof(T));
}

template<typename T>
inline void streamWrite(std::ostream &out, const T &src)
{
    out.write(reinterpret_cast<const char *>(&src), sizeof(T));
}

template<typename T>
inline void streamWrite(std::ostream &out, const std::vector<T> &src)
{
    out.write(reinterpret_cast<const char *>(&src[0]), src.size()*sizeof(T));
}

template<typename T>
inline void streamWrite(std::ostream &out, const T *dst, size_t numElements)
{
    out.write(reinterpret_cast<const char *>(dst), numElements*sizeof(T));
}

}

}

#endif /* FILEUTILS_HPP_ */
