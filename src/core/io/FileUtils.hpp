#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

#include "IntTypes.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace Tungsten
{

class FileUtils
{
public:
    static std::string loadText(const char *path);

    static void changeCurrentDir(const std::string &dir);
    static std::string getCurrentDir();

    static bool fileExists(const std::string &path);
    static bool createDirectory(const std::string &path);

    static bool copyFile(const std::string &src, const std::string &dst, bool createDstDir);

    static std::string addSlash(std::string s);
    static std::string stripSlash(std::string s);
    static std::string stripExt(std::string s);
    static std::string extractExt(std::string s);
    static std::string stripDir(std::string s);
    static std::string extractDir(std::string s);
    static std::string extractFile(std::string s);

    template<typename T>
    static inline void streamRead(std::istream &in, T &dst)
    {
        in.read(reinterpret_cast<char *>(&dst), sizeof(T));
    }

    template<typename T>
    static inline void streamRead(std::istream &in, std::vector<T> &dst)
    {
        uint64 s;
        streamRead(in, s);
        dst.resize(s);
        in.read(reinterpret_cast<char *>(&dst[0]), dst.size()*sizeof(T));
    }

    template<typename T>
    static inline void streamWrite(std::ostream &out, const T &src)
    {
        out.write(reinterpret_cast<const char *>(&src), sizeof(T));
    }

    template<typename T>
    static inline void streamWrite(std::ostream &out, const std::vector<T> &src)
    {
        streamWrite(out, uint64(src.size()));
        out.write(reinterpret_cast<const char *>(&src[0]), src.size()*sizeof(T));
    }
};

}

#endif /* FILEUTILS_HPP_ */
