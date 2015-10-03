#ifndef BITMANIP_HPP_
#define BITMANIP_HPP_

#include "IntTypes.hpp"

#include <memory>
#include <string>

namespace Tungsten {

class BitManip
{
    static struct Initializer
    {
        Initializer();
    } initializer;

    static CONSTEXPR uint32 LogMantissaBits = 16;

    static std::unique_ptr<float[]> _logLookup;

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

#if defined(__GNUC__)
    static inline uint32 msb(uint32 x)
    {
        return 32 - __builtin_clz(x);
    }
#else
    static inline uint32 msb(uint32 x)
    {
        static const uint32 table[] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

        uint32 result = 0;
        if (x & 0xFFFF0000U) { result += 16; x >>= 16; }
        if (x & 0x0000FF00U) { result +=  8; x >>=  8; }
        if (x & 0x000000F0U) { result +=  4; x >>=  4; }
        return result + table[x];
    }
#endif

    // Computes std::log(x/UINT_MAX) to within 1e-5 accuracy, but 16x faster
    static inline float normalizedLog(uint32 x)
    {
        uint32 ai = msb(x);
        uint32 res = ai < LogMantissaBits ? (x << (LogMantissaBits - ai)) : (x >> (ai - LogMantissaBits));
        return (_logLookup[res] + int(ai - LogMantissaBits - 32))*0.693147181f;
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
