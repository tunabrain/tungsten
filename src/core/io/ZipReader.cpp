#include "ZipReader.hpp"
#include "ZipStreambuf.hpp"
#include "FileUtils.hpp"

#include "Debug.hpp"

namespace Tungsten {

static size_t minizFileReadFunc(void *userPtr, mz_uint64 file_ofs, void *pBuf, size_t n)
{
    std::istream &in = *static_cast<std::istream *>(userPtr);
    if (std::streampos(file_ofs) != in.tellg())
        in.seekg(file_ofs);
    in.read(reinterpret_cast<char *>(pBuf), n);
    return size_t(in.gcount());
}

ZipReader::ZipReader(const Path &p)
: _path(p),
  _in(FileUtils::openInputStream(p))
{
    if (!_in)
        FAIL("Failed to construct ZipReader: Input stream is invalid");
    uint64 size = FileUtils::fileSize(p);
    if (size == 0)
        FAIL("Failed to construct ZipReader: Error reading file size");

    std::memset(&_archive, 0, sizeof(mz_zip_archive));
    _archive.m_pRead = &minizFileReadFunc;
    _archive.m_pIO_opaque = _in.get();
    if (!mz_zip_reader_init(&_archive, size, 0))
        FAIL("Initializing zip reader failed");

    int count = mz_zip_reader_get_num_files(&_archive);
    for (int i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        mz_zip_reader_file_stat(&_archive, i, &stat);

        ZipEntry entry;
        entry.fullPath = Path(stat.m_filename);
        entry.name = entry.fullPath.fileName();
        entry.size = uint32(stat.m_uncomp_size);
        entry.isDirectory = !entry.fullPath.empty() && entry.fullPath.asString().back() == '/';
        entry.archiveIndex = i;

        addPath(Path(stat.m_filename).stripSeparator(), std::move(entry));
    }
}

int ZipReader::addPath(const Path &p, ZipEntry entry)
{
    auto iter = _pathToEntry.find(p);
    if (iter == _pathToEntry.end()) {
        int index = int(_entries.size());

        _pathToEntry.insert(std::make_pair(p, index));
        _entries.emplace_back(std::move(entry));

        if (p != ".") {
            Path parent = p.parent().stripSeparator();
            if (parent.empty())
                parent = ".";

            ZipEntry parentEntry;
            parentEntry.fullPath = parent;
            parentEntry.name = parent.fileName();
            parentEntry.size = 0;
            parentEntry.isDirectory = true;
            parentEntry.archiveIndex = -1;

            _entries[addPath(parent, std::move(parentEntry))].contents.push_back(index);
        }

        return index;
    } else {
        return iter->second;
    }
}

const ZipEntry *ZipReader::findEntry(const Path &p) const
{
    auto iter = _pathToEntry.find(p);
    if (iter == _pathToEntry.end())
        return nullptr;
    else
        return &_entries[iter->second];
}

std::unique_ptr<ZipInputStreambuf> ZipReader::openStreambuf(const ZipEntry &entry)
{
    std::unique_ptr<ZipInputStreambuf> result;
    try {
        result.reset(new ZipInputStreambuf(_in, _archive, entry));
    } catch (const std::runtime_error &) {
        return nullptr;
    }
    return std::move(result);
}

}
