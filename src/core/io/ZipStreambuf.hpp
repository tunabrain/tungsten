#ifndef ZIPSTREAMBUF_HPP_
#define ZIPSTREAMBUF_HPP_

#include "io/FileUtils.hpp"

#include "IntTypes.hpp"

#include <miniz/miniz.h>
#include <streambuf>
#include <memory>

namespace Tungsten {

struct ZipEntry;

class ZipInputStreambuf : public std::basic_streambuf<char>
{
    InputStreamHandle _in;
    mz_zip_file_header _header;
    tinfl_decompressor _inflator;
    uint64 _uncompressedSize;
    uint64 _compressedSize;

    uint64 _inputStreamOffset;
    uint64 _outputStreamOffset;
    size_t _inputAvail;
    size_t _inputBufOffset;
    size_t _outputBufOffset;
    int64 _seekOffset;

    int _status;

    std::unique_ptr<uint8[]> _inputBuffer;
    std::unique_ptr<uint8[]> _outputBuffer;

    int_type underflow() override final;

    std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override final;
    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
            std::ios_base::openmode which) override final;

public:
    ZipInputStreambuf(InputStreamHandle in, mz_zip_archive &archive, const ZipEntry &entry);
};

}

#endif /* ZIPSTREAMBUF_HPP_ */
