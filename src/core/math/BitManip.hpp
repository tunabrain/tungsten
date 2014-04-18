#ifndef BITMANIP_HPP_
#define BITMANIP_HPP_

#include "IntTypes.hpp"

namespace Tungsten
{

class BitManip
{
public:
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

    static inline float normalizedUint(uint32 i)
    {
        return uintBitsToFloat((i >> 9u) | 0x3F800000u) - 1.0f;
    }
};

}

#endif /* BITMANIP_HPP_ */
