#include "FileStreambuf.hpp"

#include <cstring>

namespace Tungsten {

CONSTEXPR size_t FileInputStreambuf::PutBackSize;
CONSTEXPR size_t FileInputStreambuf::BufferSize;

static const std::streampos FailurePos(-1);

FileInputStreambuf::FileInputStreambuf(AutoFilePtr file)
: _file(std::move(file)),
  _buffer(new char[BufferSize])
{
    char *end = _buffer.get() + BufferSize;
    setg(end, end, end);
}

std::streambuf::int_type FileInputStreambuf::underflow()
{
    if (gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    char *start = _buffer.get();
    char *end = start + BufferSize;

    if (eback() == _buffer.get()) {
        std::memmove(_buffer.get(), egptr() - PutBackSize, PutBackSize);
        start += PutBackSize;
    }
    size_t n = std::fread(start, 1, end - start, _file.get());
    if (n == 0)
        return traits_type::eof();

    setg(_buffer.get(), start, start + n);

    return traits_type::to_int_type(*gptr());
}

std::streampos FileInputStreambuf::seekpos(std::streampos pos, std::ios_base::openmode /*which*/)
{
    char *end = _buffer.get() + BufferSize;
    setg(end, end, end);
#if _WIN32
    return _fseeki64(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#elif __APPLE__
    return fseeko(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#else
    return fseeko64(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#endif
}

std::streampos FileInputStreambuf::seekoff(std::streamoff off, std::ios_base::seekdir way,
        std::ios_base::openmode /*which*/)
{
    int whence =
        way == std::ios_base::beg ? SEEK_SET :
        way == std::ios_base::cur ? SEEK_CUR :
                                    SEEK_END;

    if (way != std::ios_base::cur || off != 0) {
        char *end = _buffer.get() + BufferSize;
        setg(end, end, end);
    }

#if _WIN32
    return _fseeki64(_file.get(), off, whence) == 0 ? _ftelli64(_file.get()) : -1;
#elif __APPLE__
    return fseeko(_file.get(), off, whence) == 0 ? ftello(_file.get()) : -1;
#else
    return fseeko64(_file.get(), off, whence) == 0 ? ftello64(_file.get()) : -1;
#endif
}

FileOutputStreambuf::FileOutputStreambuf(AutoFilePtr file)
: _file(std::move(file)),
  _buffer(new char[BufferSize])
{
    setp(_buffer.get(), _buffer.get() + BufferSize - 1);
}

FileOutputStreambuf::~FileOutputStreambuf()
{
    sync();
}

std::streambuf::int_type FileOutputStreambuf::overflow(int_type ch)
{
    if (ch != traits_type::eof()) {
        *pptr() = ch;
        pbump(1);
        return sync() == 0 ? ch : traits_type::eof();
    } else {
        return traits_type::eof();
    }
}

int FileOutputStreambuf::sync()
{
    std::ptrdiff_t n = pptr() - pbase();
    if (n == 0)
        return 0;

    pbump(int(-n));

    return std::fwrite(_buffer.get(), 1, n, _file.get()) != size_t(n) ? -1 : 0;
}

std::streampos FileOutputStreambuf::seekpos(std::streampos pos, std::ios_base::openmode /*which*/)
{
    sync();
#if _WIN32
    return _fseeki64(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#elif __APPLE__
    return fseeko(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#else
    return fseeko64(_file.get(), pos, SEEK_SET) == 0 ? pos : FailurePos;
#endif
}

std::streampos FileOutputStreambuf::seekoff(std::streamoff off, std::ios_base::seekdir way,
        std::ios_base::openmode /*which*/)
{
    int whence =
        way == std::ios_base::beg ? SEEK_SET :
        way == std::ios_base::cur ? SEEK_CUR :
                                    SEEK_END;
    sync();

#if _WIN32
    return _fseeki64(_file.get(), off, whence) == 0 ? _ftelli64(_file.get()) : -1;
#elif __APPLE__
    return fseeko(_file.get(), off, whence) == 0 ? ftello(_file.get()) : -1;
#else
    return fseeko64(_file.get(), off, whence) == 0 ? ftello64(_file.get()) : -1;
#endif
}

}
