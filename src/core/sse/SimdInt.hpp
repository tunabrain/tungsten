#ifndef SIMDINT_HPP_
#define SIMDINT_HPP_

#include "SimdBool.hpp"

#include "math/IntTypes.hpp"
#include "math/MathUtil.hpp"

#include <immintrin.h>

namespace BVH {

static_assert(false, "This class is not finished and will most likely not work correctly. Slap Benedikt to finish this");

template<uint32 N>
class SimdInt
{
public:
    static constexpr uint32 n = N;
    static constexpr size_t Alignment = N*sizeof(float);

    SimdInt() = default;
    SimdInt(const SimdInt &o) = default;
    SimdInt(int32 a);

    SimdInt operator-() const;
    SimdInt operator+(const SimdInt &o) const;
    SimdInt operator-(const SimdInt &o) const;
    //SimdInt operator*(const SimdInt &o) const;
    //SimdInt operator/(const SimdInt &o) const;

    SimdInt &operator+=(const SimdInt &o);
    SimdInt &operator-=(const SimdInt &o);
    //SimdInt &operator*=(const SimdInt &o);
    //SimdInt &operator/=(const SimdInt &o);

    SimdBool<N> operator==(const SimdInt &o) const;
    SimdBool<N> operator!=(const SimdInt &o) const;
    SimdBool<N> operator<=(const SimdInt &o) const;
    SimdBool<N> operator>=(const SimdInt &o) const;
    SimdBool<N> operator<(const SimdInt &o) const;
    SimdBool<N> operator>(const SimdInt &o) const;

    int32 *data();
    const int32 *data() const;

    int32 &operator[](unsigned i);
    int32 operator[](unsigned i) const;
};

template<>
class SimdInt<1>
{
    int32 _a;
public:
    static constexpr uint32 n = 1;
    static constexpr size_t Alignment = sizeof(int32);

    SimdInt() = default;
    SimdInt(const SimdInt &o) = default;
    SimdInt(int32 a)
    : _a(a)
    {
    }

    SimdInt operator-() const { return -_a; }
    SimdInt operator+(const SimdInt &o) const { return _a + o._a; }
    SimdInt operator-(const SimdInt &o) const { return _a - o._a; }
    //SimdInt operator*(const SimdInt &o) const { return _a * o._a; }
    //SimdInt operator/(const SimdInt &o) const { return _a / o._a; }

    SimdInt &operator+=(const SimdInt &o) { _a += o._a; return *this; }
    SimdInt &operator-=(const SimdInt &o) { _a -= o._a; return *this; }
    //SimdInt &operator*=(const SimdInt &o) { _a *= o._a; return *this; }
    //SimdInt &operator/=(const SimdInt &o) { _a /= o._a; return *this; }

    SimdBool<1> operator==(const SimdInt &o) const { return SimdBool<1>(_a == o._a); }
    SimdBool<1> operator!=(const SimdInt &o) const { return SimdBool<1>(_a != o._a); }
    SimdBool<1> operator<=(const SimdInt &o) const { return SimdBool<1>(_a <= o._a); }
    SimdBool<1> operator>=(const SimdInt &o) const { return SimdBool<1>(_a >= o._a); }
    SimdBool<1> operator< (const SimdInt &o) const { return SimdBool<1>(_a <  o._a); }
    SimdBool<1> operator> (const SimdInt &o) const { return SimdBool<1>(_a >  o._a); }

    int32 *data() { return &_a; }
    const int32 *data() const { return &_a; };

    int32 &operator[](unsigned /*i*/) { return _a; }
    int32 operator[](unsigned /*i*/) const { return _a; }
};

#ifdef __SSE__
template<>
class SimdInt<4>
{
    __m128i _a;

    SimdInt(const __m128i &a)
    : _a(a)
    {
    }
public:
    static constexpr uint32 n = 4;
    static constexpr size_t Alignment = 4*sizeof(int32);

    SimdInt() = default;
    SimdInt(const SimdInt &o) = default;
    SimdInt(int32 a)
    : _a(_mm_set1_epi32(a))
    {
    }

    SimdInt operator-() const { return _mm_sub_epi32(_mm_setzero_ps(), _a); }
    SimdInt operator+(const SimdInt &o) const { return _mm_add_epi32(_a, o._a); }
    SimdInt operator-(const SimdInt &o) const { return _mm_sub_epi32(_a, o._a); }
    //SimdInt operator*(const SimdInt &o) const { return _mm_mul_epi32(_a, o._a); }
    //SimdInt operator/(const SimdInt &o) const { return _mm_div_epi32(_a, o._a); }

    SimdInt &operator+=(const SimdInt &o) { _a = _mm_add_epi32(_a, o._a); return *this; }
    SimdInt &operator-=(const SimdInt &o) { _a = _mm_sub_epi32(_a, o._a); return *this; }
    //SimdInt &operator*=(const SimdInt &o) { _a = _mm_mul_epi32(_a, o._a); return *this; }
    //SimdInt &operator/=(const SimdInt &o) { _a = _mm_div_epi32(_a, o._a); return *this; }

    SimdBool<4> operator==(const SimdInt &o) const { return SimdBool<4>(_mm_cmpeq_epi32 (_a, o._a)); }
    SimdBool<4> operator!=(const SimdInt &o) const { return !SimdBool<4>(_mm_cmpeq_epi32(_a, o._a)); }
    SimdBool<4> operator<=(const SimdInt &o) const { return !SimdBool<4>(_mm_cmpgt_epi32 (o._a, _a)); }
    SimdBool<4> operator>=(const SimdInt &o) const { return !SimdBool<4>(_mm_cmplt_epi32 (o._a, _a)); }
    SimdBool<4> operator< (const SimdInt &o) const { return SimdBool<4>(_mm_cmplt_epi32 (_a, o._a)); }
    SimdBool<4> operator> (const SimdInt &o) const { return SimdBool<4>(_mm_cmpgt_epi32 (_a, o._a)); }

    int32 *data() { return reinterpret_cast<int32 *>(&_a); }
    const int32 *data() const { return reinterpret_cast<const int32 *>(&_a); };

    int32 &operator[](unsigned i) { return data()[i]; }
    int32 operator[](unsigned i) const { return data()[i]; }
};
#endif

#ifdef __AVX__
template<>
class SimdInt<8>
{
    __m256 _a;

    SimdInt(const __m256 &a)
    : _a(a)
    {
    }
public:
    static constexpr uint32 n = 8;
    static constexpr size_t Alignment = 8*sizeof(float);

    SimdInt() = default;
    SimdInt(const SimdInt &o) = default;
    SimdInt(float a)
    : _a(_mm256_set1_ps(a))
    {
    }

    SimdInt operator-() const { return _mm256_sub_ps(_mm256_setzero_ps(), _a); }
    SimdInt operator+(const SimdInt &o) const { return _mm256_add_ps(_a, o._a); }
    SimdInt operator-(const SimdInt &o) const { return _mm256_sub_ps(_a, o._a); }
    SimdInt operator*(const SimdInt &o) const { return _mm256_mul_ps(_a, o._a); }
    SimdInt operator/(const SimdInt &o) const { return _mm256_div_ps(_a, o._a); }

    SimdInt &operator+=(const SimdInt &o) { _a = _mm256_add_ps(_a, o._a); return *this; }
    SimdInt &operator-=(const SimdInt &o) { _a = _mm256_sub_ps(_a, o._a); return *this; }
    SimdInt &operator*=(const SimdInt &o) { _a = _mm256_mul_ps(_a, o._a); return *this; }
    SimdInt &operator/=(const SimdInt &o) { _a = _mm256_div_ps(_a, o._a); return *this; }

    SimdBool<8> operator==(const SimdInt &o) const { return SimdBool<8>(_mm256_cmpeq_ps (_a, o._a)); }
    SimdBool<8> operator!=(const SimdInt &o) const { return SimdBool<8>(_mm256_cmpneq_ps(_a, o._a)); }
    SimdBool<8> operator<=(const SimdInt &o) const { return SimdBool<8>(_mm256_cmple_ps (_a, o._a)); }
    SimdBool<8> operator>=(const SimdInt &o) const { return SimdBool<8>(_mm256_cmpge_ps (_a, o._a)); }
    SimdBool<8> operator< (const SimdInt &o) const { return SimdBool<8>(_mm256_cmplt_ps (_a, o._a)); }
    SimdBool<8> operator> (const SimdInt &o) const { return SimdBool<8>(_mm256_cmpgt_ps (_a, o._a)); }

    float *data() { return reinterpret_cast<float *>(&_a); }
    const float *data() const { return reinterpret_cast<const float *>(&_a); };

    float sum() const {
        alignas(32) float as[8];
        _mm256_store_ps(as, _a);
        return as[0] + as[1] + as[2] + as[3] + as[4] + as[5] + as[6] + as[7];
    }

    float &operator[](unsigned i) { return data()[i]; }
    float operator[](unsigned i) const { return data()[i]; }
};
#endif

constexpr SimdInt<1> min(const SimdInt<1> &a, const SimdInt<1> &b)
{
    return a._a < b._a ? a : b;
}

constexpr SimdInt<1> max(const SimdInt<1> &a, const SimdInt<1> &b)
{
    return a._a > b._a ? a : b;
}

#ifdef __SSE__
constexpr SimdInt<4> min(const SimdInt<4> &a, const SimdInt<4> &b)
{
    return SimdInt<4>(_mm_min_ps(a._a, b._a));
}

constexpr SimdInt<4> max(const SimdInt<4> &a, const SimdInt<4> &b)
{
    return SimdInt<4>(_mm_max_ps(a._a, b._a));
}
#endif

#ifdef __AVX__
constexpr SimdInt<8> min(const SimdInt<8> &a, const SimdInt<8> &b)
{
    return SimdInt<8>(_mm256_min_ps(a._a, b._a));
}

constexpr SimdInt<8> max(const SimdInt<8> &a, const SimdInt<8> &b)
{
    return SimdInt<8>(_mm256_max_ps(a._a, b._a));
}
#endif

}



#endif /* SIMDINT_HPP_ */
