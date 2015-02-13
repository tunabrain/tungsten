#ifndef FILEITERATOR_HPP_
#define FILEITERATOR_HPP_

#include "FileUtils.hpp"
#include "Path.hpp"

#include <memory>

namespace Tungsten {

class FileIterator
{
    Path _dir;
    bool _ignoreFiles;
    bool _ignoreDirectories;
    Path _extensionFilter;

    std::shared_ptr<OpenDir> _openDir;

    Path _currentEntry;

public:
    FileIterator();
    FileIterator(const Path &p, bool ignoreFiles, bool ignoreDirectories, const Path &extensionFilter);

    bool operator==(const FileIterator &o) const;
    bool operator!=(const FileIterator &o) const;

    FileIterator &operator++();
    FileIterator  operator++(int);

    Path &operator*();
    const Path &operator*() const;
};

}



#endif /* FILEITERATOR_HPP_ */
