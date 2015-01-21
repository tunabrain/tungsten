#ifndef FILE_HPP_
#define FILE_HPP_

#include "io/FileUtils.hpp"

#include <functional>
#include <sys/stat.h>
#include <dirent.h>

namespace Tungsten {

class File
{
    std::string _path;
    DIR *_openDir = nullptr;
    std::function<bool(const File &)> _filter;

public:
    File() = default;

    explicit File(const std::string &path)
    : _path(path)
    {
        if (!path.empty() && path.back() == ':')
            _path += '/';
    }

    ~File()
    {
        if (_openDir)
            closedir(_openDir);
    }

    bool valid() const
    {
        return !_path.empty();
    }

    bool exists() const
    {
        if(_path.empty())
            return false;
        struct stat buffer;
        return (stat(_path.c_str(), &buffer) == 0);
    }

    bool isDirectory() const
    {
        struct stat buffer;
        if (stat(_path.c_str(), &buffer) == -1)
            return false;
        return S_ISDIR(buffer.st_mode); //S_IS
    }

    bool isFile() const
    {
        struct stat buffer;
        if (stat(_path.c_str(), &buffer) == -1)
            return false;
        return S_ISREG(buffer.st_mode);
    }

    void endScan()
    {
        if (_openDir) {
            closedir(_openDir);
            _openDir = nullptr;
        }
        _filter = nullptr;
    }

    File beginScan()
    {
        endScan();
        _openDir = opendir(_path.c_str());
        return scan();
    }

    File beginScan(std::function<bool(const File &)> filter)
    {
        endScan();
        _filter = filter;
        _openDir = opendir(_path.c_str());
        return scan();
    }

    File scan()
    {
        if (!_openDir)
            return File();

        while (true) {
            dirent *entry = readdir(_openDir);
            if (!entry) {
                endScan();
                return File();
            }

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            File file(FileUtils::addSeparator(_path) + entry->d_name);
            if (_filter && !_filter(file))
                continue;

            return file;
        }
    }

    File file(const std::string &fileName) const
    {
        return File(concat(fileName));
    }

    const std::string &path() const
    {
        return _path;
    }

    std::string name() const
    {
        return FileUtils::stripParent(_path);
    }

    std::string baseName() const
    {
        return FileUtils::extractBase(_path);
    }

    std::string ext() const
    {
        return FileUtils::extractExt(_path);
    }

    std::string concat(const std::string &o) const
    {
        return FileUtils::addSeparator(_path) + o;
    }

    static std::function<bool(const File &)> extFilter(const std::string &ext)
    {
        return [ext](const File &file) { return file.isFile() && strcasecmp(file.ext().c_str(), ext.c_str()) == 0; };
    }

    static std::function<bool(const File &)> dirFilter()
    {
        return [](const File &file) { return file.isDirectory(); };
    }

    static std::function<bool(const File &)> fileFilter()
    {
        return [](const File &file) { return file.isFile(); };
    }
};

}

#endif /* FILE_HPP_ */
