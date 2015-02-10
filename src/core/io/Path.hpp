#ifndef PATH_HPP_
#define PATH_HPP_

#include <ostream>
#include <string>
#include <memory>

namespace Tungsten {

class Path
{
    std::string _workingDirectory;
    std::string _path;

    Path(const std::string &workingDirectory, const std::string &path);

public:
    Path() = default;
    Path(const std::string &path);

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

    std::string absolutePath() const;
    const std::string &path() const;

    Path extension() const;
    Path fileName() const;
    Path baseName() const;
    Path parent() const;
    Path stripParent() const;
    Path stripExtension() const;
    Path setExtension(const Path &ext) const;

    void ensureSeparator();
    void stripSeparator();

    Path &operator/=(const Path &o);
    Path &operator+=(const Path &o);
    Path &operator/=(const std::string &o);
    Path &operator+=(const std::string &o);

    Path operator/(const Path &o) const;
    Path operator+(const Path &o) const;
    Path operator/(const std::string &o) const;
    Path operator+(const std::string &o) const;

    friend std::ostream &operator<< (std::ostream &stream, const Path &p) {
        return stream << p._path;
    }
};

typedef std::shared_ptr<Path> PathPtr;

}

#endif /* PATH_HPP_ */
