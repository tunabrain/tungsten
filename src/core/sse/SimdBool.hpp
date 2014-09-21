#ifndef SIMDBOOL_HPP_
#define SIMDBOOL_HPP_

#include "math/MathUtil.hpp"

#include "IntTypes.hpp"

#include <immintrin.h>

namespace Tungsten {

template<uint32 N> class SimdFloat;

template<uint32 N>
class SimdBool
{
public:
    static CONSTEXPR uint32 n = N;
    static CONSTEXPR size_t Alignment = N*sizeof(float);

    SimdBool() = default;
    SimdBool(const SimdBool &o) = default;
    SimdBool(bool a);

    bool any() const;
    bool all() const;

    SimdBool operator!() const;
    SimdBool operator||(const SimdBool &o) const;
    SimdBool operator&&(const SimdBool &o) const;
};

template<>
class SimdBool<1>
{
    bool _b;

    friend SimdFloat<1>;
public:
    static CONSTEXPR uint32 n = 1;
    static CONSTEXPR size_t Alignment = sizeof(bool);

    SimdBool() = default;
    SimdBool(const SimdBool &o) = default;
    SimdBool(bool a)
    : _b(a)
    {
    }

    bool any() const { return _b; }
    bool all() const { return _b; }

    SimdBool operator!() const { return !_b; }
    SimdBool operator||(const SimdBool &o) const { return _b || o._b; }
    SimdBool operator&&(const SimdBool &o) const { return _b && o._b; }

    bool operator[](uint32 /*idx*/) { return _b; };
};

#ifdef __SSE__
template<>
class SimdBool<4>
{
    union {
        __m128 _b;
        uint32 _i[4];
    };

    friend SimdFloat<4>;
public:
    static CONSTEXPR uint32 n = 4;
    static CONSTEXPR size_t Alignment = 4*sizeof(float);

    SimdBool() = default;
    SimdBool(const SimdBool &o) = default;
    SimdBool(const __m128 &a)
    : _b(a)
    {
    }
    SimdBool(bool a)
    : _b(_mm_set1_ps(a ? BitManip::uintBitsToFloat(0xFFFFFFFF) : 0))
    {
    }

    bool any() const { return _mm_movemask_ps(_b) != 0; }
    bool all() const { return _mm_movemask_ps(_b) == 0xF; }

    SimdBool operator!() const { return _mm_xor_ps(_mm_set1_ps(BitManip::uintBitsToFloat(0xFFFFFFFF)), _b); }
    SimdBool operator||(const SimdBool &o) const { return _mm_or_ps(_b, o._b); }
    SimdBool operator&&(const SimdBool &o) const { return _mm_and_ps(_b, o._b); }

    bool operator[](uint32 idx) { return _i[idx] != 0; };
};

typedef SimdBool<4> bool4;

#endif

#ifdef __AVX__
template<>
class SimdBool<8>
{
    union {
        __m256 _b;
        uint32 _i[8];
    };

    friend SimdFloat<8>;
public:
    static CONSTEXPR uint32 n = 8;
    static CONSTEXPR size_t Alignment = 8*sizeof(float);

    SimdBool() = default;
    SimdBool(const SimdBool &o) = default;
    SimdBool(const __m256 &a)
    : _b(a)
    {
    }
    SimdBool(bool a)
    : _b(_mm256_set1_ps(a ? BitManip::uintBitsToFloat(0xFFFFFFFF) : 0))
    {
    }

    bool any() const { return _mm256_movemask_ps(_b) != 0; }
    bool all() const { return _mm256_movemask_ps(_b) == 0xFF; }

    SimdBool operator!() const { return _mm256_xor_ps(_mm256_set1_ps(BitManip::uintBitsToFloat(0xFFFFFFFF)), _b); }
    SimdBool operator||(const SimdBool &o) const { return _mm256_or_ps(_b, o._b); }
    SimdBool operator&&(const SimdBool &o) const { return _mm256_and_ps(_b, o._b); }

    bool operator[](uint32 idx) { return _i[idx]; };
};

typedef SimdBool<8> bool8;

#endif

}

#endif /* SIMDBOOL_HPP_ */
