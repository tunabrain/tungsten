#ifndef FILEITERABLES_HPP_
#define FILEITERABLES_HPP_

#include "FileIterator.hpp"

namespace Tungsten {

class FileIterable
{
    Path _path;
    Path _extension;

public:
    FileIterable(const Path &p, const Path &extension)
    : _path(p),
      _extension(extension)
    {
    }

    FileIterator begin() const
    {
        return FileIterator(_path, false, true, _extension);
    }

    FileIterator end() const
    {
        return FileIterator();
    }
};

class DirectoryIterable
{
    Path _path;

public:
    DirectoryIterable(const Path &p)
    : _path(p)
    {
    }

    FileIterator begin() const
    {
        return FileIterator(_path, true, false, Path());
    }

    FileIterator end() const
    {
        return FileIterator();
    }
};

}

#endif /* FILEITERABLES_HPP_ */
