#include "ZipWriter.hpp"
#include "Debug.hpp"
#include "Path.hpp"

#include <cstring>

namespace Tungsten {

static size_t zipStreamWriteFunc(void *userPtr, mz_uint64 file_ofs, const void *pBuf, size_t n)
{
    std::ostream &out = *static_cast<std::ostream *>(userPtr);
    size_t pos = size_t(out.tellp());
    if (file_ofs != pos)
        out.seekp(file_ofs);
    if (!out.good())
        return 0;
    out.write(reinterpret_cast<const char *>(pBuf), n);
    return out.good() ? n : 0;
}

ZipWriter::ZipWriter(const Path &dst)
: ZipWriter(FileUtils::openOutputStream(dst))
{
}

ZipWriter::ZipWriter(OutputStreamHandle dst)
: _out(std::move(dst))
{
    if (!_out)
        FAIL("Failed to construct ZipWriter: Output stream is invalid");
    std::memset(&_archive, 0, sizeof(mz_zip_archive));
    _archive.m_pWrite = &zipStreamWriteFunc;
    _archive.m_pIO_opaque = _out.get();
    if (!mz_zip_writer_init(&_archive, 0))
        FAIL("Initializing zip writer failed");
}

ZipWriter::~ZipWriter()
{
    close();
}

bool ZipWriter::addFile(const Path &src, const Path &dst, int compressionLevel)
{
    return mz_zip_writer_add_file(&_archive, dst.asString().c_str(), src.absolute().asString().c_str(),
            0, 0, compressionLevel) != 0;
}

bool ZipWriter::addFile(const void *src, size_t len, const Path &dst, int compressionLevel)
{
    return mz_zip_writer_add_mem(&_archive, dst.asString().c_str(), src, len, compressionLevel) != 0;
}

bool ZipWriter::addDirectory(const Path &dst)
{
    return mz_zip_writer_add_mem(&_archive, dst.ensureSeparator().asString().c_str(), nullptr, 0, 0) != 0;
}

void ZipWriter::close()
{
    if (_out) {
        mz_zip_writer_finalize_archive(&_archive);
        mz_zip_writer_end(&_archive);
        _out.reset();
    }
}

}
