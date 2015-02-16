#ifndef BITMANIP_HPP_
#define BITMANIP_HPP_

#include "IntTypes.hpp"

#include <string>

namespace Tungsten {

class BitManip
{
public:
    // Note: Could replace this with memcpy, which gcc optimizes to the same assembly
    // as the code below. I'm not sure how other compiler treat it though, since it's
    // really part of the C runtime. The union seems to be portable enough.
    static inline float uintBitsToFloat(uint32 i)
    {
        union {
            float f;
            uint32 i;
        } unionHack;
        unionHack.i = i;
        return unionHack.f;
    }

    static inline uint32 floatBitsToUint(float f)
    {
        union {
            float f;
            uint32 i;
        } unionHack;
        unionHack.f = f;
        return unionHack.i;
    }

    // 2x-5x faster than i/float(UINT_MAX)
    static inline float normalizedUint(uint32 i)
    {
        return uintBitsToFloat((i >> 9u) | 0x3F800000u) - 1.0f;
    }

    // std::hash is not necessarily portable across compilers.
    // In case a consistent hash function is needed (for file IO),
    // this will do. This function is what is used in gawk.
    // See also http://www.cse.yorku.ca/~oz/hash.html
    static inline uint64 hash(const std::string &s)
    {
        uint64 result = 0;
        for (char c : s)
            result = (result*65599ull) + uint64(c);
        return result;
    }
};

}

#endif /* BITMANIP_HPP_ */
