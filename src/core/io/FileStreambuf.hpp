#ifndef FILESTREAMBUF_HPP_
#define FILESTREAMBUF_HPP_

#include <streambuf>
#include <cstdio>
#include <memory>

namespace Tungsten {

typedef std::unique_ptr<FILE, int(*)(FILE*)> AutoFilePtr;

class FileInputStreambuf : public std::basic_streambuf<char>
{
    static CONSTEXPR size_t PutBackSize = 8;
    static CONSTEXPR size_t BufferSize = 8*1024;

    AutoFilePtr _file;
    std::unique_ptr<char[]> _buffer;

    int_type underflow() override final;

    std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override final;
    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
            std::ios_base::openmode which) override final;

public:
    FileInputStreambuf(AutoFilePtr file);
};

class FileOutputStreambuf : public std::basic_streambuf<char>
{
    static CONSTEXPR size_t BufferSize = 8*1024;

    AutoFilePtr _file;
    std::unique_ptr<char[]> _buffer;

    int_type overflow(int_type ch) override final;
    int sync() override final;

    std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override final;
    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
            std::ios_base::openmode which) override final;

public:
    FileOutputStreambuf(AutoFilePtr file);
    ~FileOutputStreambuf();
};

}

#endif /* FILESTREAMBUF_HPP_ */
