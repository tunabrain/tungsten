#include "FileIterator.hpp"

#ifdef _MSC_VER
#include <dirent/dirent.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif
#include <cstring>

#include <iostream>

namespace Tungsten {

FileIterator::FileIterator()
: _openDir(nullptr)
{
}

FileIterator::FileIterator(const Path &p, bool ignoreFiles, bool ignoreDirectories, const Path &extensionFilter)
: _dir(p),
  _ignoreFiles(ignoreFiles),
  _ignoreDirectories(ignoreDirectories),
  _extensionFilter(extensionFilter),
  _openDir(FileUtils::openDirectory(p))
{
    (*this)++;
}

bool FileIterator::operator==(const FileIterator &o) const
{
    return _openDir == o._openDir;
}

bool FileIterator::operator!=(const FileIterator &o) const
{
    return !((*this) == o);
}

FileIterator &FileIterator::operator++()
{
    if (_openDir) {
        bool succeeded = _openDir->increment(_currentEntry, _dir, [&](const Path &p) {
            if (_ignoreFiles && p.isFile())
                return false;
            if (_ignoreDirectories && p.isDirectory())
                return false;
            if (!_extensionFilter.empty() && !p.testExtension(_extensionFilter))
                return false;
            return true;
        });
        if (!succeeded)
            _openDir.reset();
    }

    return *this;
}

FileIterator FileIterator::operator++(int)
{
    FileIterator copy(*this);
    ++(*this);
    return copy;
}

Path &FileIterator::operator*()
{
    return _currentEntry;
}

const Path &FileIterator::operator*() const
{
    return _currentEntry;
}

}
