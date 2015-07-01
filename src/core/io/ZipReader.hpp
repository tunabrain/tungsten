#ifndef ZIPREADER_HPP_
#define ZIPREADER_HPP_

#include "FileUtils.hpp"
#include "ZipEntry.hpp"
#include "Path.hpp"

#include <miniz/miniz.h>
#include <unordered_map>
#include <vector>

namespace Tungsten {

class ZipInputStreambuf;

class ZipReader
{
    Path _path;

    std::unordered_map<Path, int> _pathToEntry;
    std::vector<ZipEntry> _entries;

    mz_zip_archive _archive;
    InputStreamHandle _in;

    int addPath(const Path &p, ZipEntry entry);

public:
    ZipReader(const Path &p);

    std::unique_ptr<ZipInputStreambuf> openStreambuf(const ZipEntry &entry);

    const ZipEntry *findEntry(const Path &p) const;

    const ZipEntry &entry(int idx) const
    {
        return _entries[idx];
    }

    const Path &path() const
    {
        return _path;
    }
};

}

#endif /* ZIPREADER_HPP_ */
