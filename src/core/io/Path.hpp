#ifndef PATH_HPP_
#define PATH_HPP_

#include <ostream>
#include <string>
#include <memory>

namespace Tungsten {

class RecursiveIterable;
class DirectoryIterable;
class FileIterable;
class FileIterator;

class Path
{
    std::string _workingDirectory;
    std::string _path;

    Path(const std::string &workingDirectory, const std::string &path);

public:
    Path() = default;
    Path(const Path &workingDirectory, const std::string &path);
    explicit Path(const std::string &path);
    Path(const char *path);

    bool testExtension(const Path &ext) const;
    bool isRootDirectory() const;
    bool isAbsolute() const;
    bool isRelative() const;
    bool isDirectory() const;
    bool isFile() const;
    bool exists() const;
    bool empty() const;
    size_t size() const;

    void freezeWorkingDirectory();
    void clearWorkingDirectory();
    void setWorkingDirectory(const Path &dir);

    const std::string &asString() const;

    Path extension() const;
    Path fileName() const;
    Path baseName() const;
    Path parent() const;
    Path stripParent() const;
    Path stripExtension() const;
    Path setExtension(const Path &ext) const;
    Path absolute() const;
    Path normalizeSeparators() const;
    Path nativeSeparators() const;
    Path ensureSeparator() const;
    Path stripSeparator() const;
    Path normalize() const;

    Path &operator/=(const Path &o);
    Path &operator+=(const Path &o);
    Path &operator/=(const std::string &o);
    Path &operator+=(const std::string &o);
    Path &operator/=(const char *o);
    Path &operator+=(const char *o);

    Path operator/(const Path &o) const;
    Path operator+(const Path &o) const;
    Path operator/(const std::string &o) const;
    Path operator+(const std::string &o) const;
    Path operator/(const char *o) const;
    Path operator+(const char *o) const;

    bool operator==(const Path &o) const;
    bool operator!=(const Path &o) const;
    bool operator<(const Path &o) const;

    FileIterator begin() const;
    FileIterator end() const;
    FileIterable files(const Path &extensionFilter = Path()) const;
    DirectoryIterable directories() const;
    RecursiveIterable recursive() const;

    friend std::ostream &operator<< (std::ostream &stream, const Path &p) {
        return stream << p._path;
    }
};

typedef std::shared_ptr<Path> PathPtr;

}

namespace std {

template<> struct hash<Tungsten::Path>
{
    std::size_t operator()(const Tungsten::Path &v) const {
        return std::hash<std::string>()(v.asString());
    }
};

}

#endif /* PATH_HPP_ */
