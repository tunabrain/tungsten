#ifndef MEMBUF_HPP_
#define MEMBUF_HPP_

#include <streambuf>

using std::basic_streambuf;

namespace Tungsten {

class MemBuf : public basic_streambuf<char>
{
public:
    MemBuf(char *p, size_t n)
    {
        setg(p, p, p + n);
        setp(p, p + n);
    }
};

}

#endif /* MEMBUF_HPP_ */
