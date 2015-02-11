#include "ZipWriter.hpp"
#include "Debug.hpp"
#include "Path.hpp"

#include <cstring>

namespace Tungsten {

ZipWriter::ZipWriter(const Path &dst)
: _open(false)
{
    std::memset(&_archive, 0, sizeof(mz_zip_archive));
    if (!mz_zip_writer_init_file(&_archive, dst.absolute().asString().c_str(), 0))
        FAIL("Writing zip file at %s failed", dst.absolute());
    _open = true;
}

ZipWriter::~ZipWriter()
{
    close();
}

bool ZipWriter::addFile(const Path &src, const Path &dst, int compressionLevel)
{
    return mz_zip_writer_add_file(&_archive, dst.asString().c_str(), src.absolute().asString().c_str(),
            0, 0, compressionLevel);
}

bool ZipWriter::addFile(const void *src, size_t len, const Path &dst, int compressionLevel)
{
    return mz_zip_writer_add_mem(&_archive, dst.asString().c_str(), src, len, compressionLevel);
}

bool ZipWriter::addDirectory(const Path &dst)
{
    return mz_zip_writer_add_mem(&_archive, dst.ensureSeparator().asString().c_str(), nullptr, 0, 0);
}

void ZipWriter::close()
{
    if (_open) {
        _open = false;
        mz_zip_writer_finalize_archive(&_archive);
        mz_zip_writer_end(&_archive);
    }
}

}
