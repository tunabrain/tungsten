#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

#include "IntTypes.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace Tungsten {

// WARNING: Do not assume any functions operating on paths to be thread-safe or re-entrant.
// The underlying operating system API as well as the implementation here do not make this safe.
namespace FileUtils {

bool isRootDirectory(const std::string &path);
bool isRelativePath(const std::string &path);

std::string addSeparator(std::string path);
std::string stripSeparator(std::string path);

std::string stripExt(std::string path);
std::string stripParent(std::string path);

std::string extractExt(std::string path);
std::string extractParent(std::string path);
std::string extractBase(std::string path);

bool testExtension(const std::string &path, const std::string &ext);

std::string toAbsolutePath(const std::string &path);

bool changeCurrentDir(const std::string &dir);
std::string getCurrentDir();

std::string getExecutablePath();

bool fileExists(const std::string &path);
bool createDirectory(const std::string &path, bool recursive = true);

std::string loadText(const std::string &path);

bool copyFile(const std::string &src, const std::string &dst, bool createDstDir);

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
inline void streamWrite(std::ostream &out, const T &src)
{
    out.write(reinterpret_cast<const char *>(&src), sizeof(T));
}

template<typename T>
inline void streamWrite(std::ostream &out, const std::vector<T> &src)
{
    out.write(reinterpret_cast<const char *>(&src[0]), src.size()*sizeof(T));
}

}

}

#endif /* FILEUTILS_HPP_ */
