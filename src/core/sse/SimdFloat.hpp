#ifndef SIMDFLOAT_HPP_
#define SIMDFLOAT_HPP_

#include "SimdBool.hpp"

#include "math/MathUtil.hpp"

#include "IntTypes.hpp"

#include <immintrin.h>

namespace Tungsten {

template<uint32 N>
class SimdFloat
{
public:
    static CONSTEXPR uint32 n = N;
    static CONSTEXPR size_t Alignment = N*sizeof(float);

    SimdFloat() = default;
    SimdFloat(const SimdFloat &o) = default;
    SimdFloat(const float *a);
    SimdFloat(float a);

    SimdFloat operator-() const;
    SimdFloat operator+(const SimdFloat &o) const;
    SimdFloat operator-(const SimdFloat &o) const;
    SimdFloat operator*(const SimdFloat &o) const;
    SimdFloat operator/(const SimdFloat &o) const;

    SimdFloat operator&(const SimdBool<N> &o) const;
    SimdFloat blend(const SimdFloat &o, const SimdBool<N> &mask) const;

    SimdFloat &operator+=(const SimdFloat &o);
    SimdFloat &operator-=(const SimdFloat &o);
    SimdFloat &operator*=(const SimdFloat &o);
    SimdFloat &operator/=(const SimdFloat &o);

    SimdBool<N> operator==(const SimdFloat &o) const;
    SimdBool<N> operator!=(const SimdFloat &o) const;
    SimdBool<N> operator<=(const SimdFloat &o) const;
    SimdBool<N> operator>=(const SimdFloat &o) const;
    SimdBool<N> operator<(const SimdFloat &o) const;
    SimdBool<N> operator>(const SimdFloat &o) const;

    float *data();
    const float *data() const;

    float sum() const;

    float &operator[](unsigned i);
    float operator[](unsigned i) const;
};

template<>
class SimdFloat<1>
{
    float _a;

    friend SimdFloat<1> min(const Tungsten::SimdFloat<1> &, const Tungsten::SimdFloat<1> &);
    friend SimdFloat<1> max(const Tungsten::SimdFloat<1> &, const Tungsten::SimdFloat<1> &);
    friend SimdFloat<1> sqrt(const Tungsten::SimdFloat<1> &);
public:
    static CONSTEXPR uint32 n = 1;
    static CONSTEXPR size_t Alignment = sizeof(float);

    SimdFloat() = default;
    SimdFloat(const SimdFloat &o) = default;
    SimdFloat(const float *a)
    : _a(*a)
    {
    }
    SimdFloat(float a)
    : _a(a)
    {
    }

    inline SimdFloat operator-() const { return -_a; }
    inline SimdFloat operator+(const SimdFloat &o) const { return _a + o._a; }
    inline SimdFloat operator-(const SimdFloat &o) const { return _a - o._a; }
    inline SimdFloat operator*(const SimdFloat &o) const { return _a * o._a; }
    inline SimdFloat operator/(const SimdFloat &o) const { return _a / o._a; }

    inline SimdFloat operator&(const SimdBool<1> &o) const { return o.any() ? _a : 0.0f; }
    SimdFloat blend(const SimdFloat &o, const SimdBool<1> &mask) const { return mask.any() ? o._a : _a; }

    inline SimdFloat &operator+=(const SimdFloat &o) { _a += o._a; return *this; }
    inline SimdFloat &operator-=(const SimdFloat &o) { _a -= o._a; return *this; }
    inline SimdFloat &operator*=(const SimdFloat &o) { _a *= o._a; return *this; }
    inline SimdFloat &operator/=(const SimdFloat &o) { _a /= o._a; return *this; }

    inline SimdBool<1> operator==(const SimdFloat &o) const { return SimdBool<1>(_a == o._a); }
    inline SimdBool<1> operator!=(const SimdFloat &o) const { return SimdBool<1>(_a != o._a); }
    inline SimdBool<1> operator<=(const SimdFloat &o) const { return SimdBool<1>(_a <= o._a); }
    inline SimdBool<1> operator>=(const SimdFloat &o) const { return SimdBool<1>(_a >= o._a); }
    inline SimdBool<1> operator< (const SimdFloat &o) const { return SimdBool<1>(_a <  o._a); }
    inline SimdBool<1> operator> (const SimdFloat &o) const { return SimdBool<1>(_a >  o._a); }

    float *data() { return &_a; }
    const float *data() const { return &_a; };

    float sum() const { return _a; }

    float &operator[](unsigned /*i*/) { return _a; }
    float operator[](unsigned /*i*/) const { return _a; }
};

#ifdef __SSE__
template<>
class SimdFloat<4>
{
    union {
        __m128 _a;
        float _f[4];
        uint32 _i[4];
    };

    friend SimdFloat<4> min(const Tungsten::SimdFloat<4> &, const Tungsten::SimdFloat<4> &);
    friend SimdFloat<4> max(const Tungsten::SimdFloat<4> &, const Tungsten::SimdFloat<4> &);
    friend SimdFloat<4> sqrt(const Tungsten::SimdFloat<4> &);
public:
    static CONSTEXPR uint32 n = 4;
    static CONSTEXPR size_t Alignment = 4*sizeof(float);

    SimdFloat() = default;
    SimdFloat(const SimdFloat &o) = default;
    SimdFloat(const float *a)
    : _a(_mm_load_ps(a))
    {
    }
    SimdFloat(float a)
    : _a(_mm_set1_ps(a))
    {
    }
    SimdFloat(float r0, float r1, float r2, float r3)
    : _a(_mm_set_ps(r3, r2, r1, r0))
    {
    }
    SimdFloat(const __m128 &a)
    : _a(a)
    {
    }

    SimdFloat operator-() const { return _mm_sub_ps(_mm_setzero_ps(), _a); }
    SimdFloat operator+(const SimdFloat &o) const { return _mm_add_ps(_a, o._a); }
    SimdFloat operator-(const SimdFloat &o) const { return _mm_sub_ps(_a, o._a); }
    SimdFloat operator*(const SimdFloat &o) const { return _mm_mul_ps(_a, o._a); }
    SimdFloat operator/(const SimdFloat &o) const { return _mm_div_ps(_a, o._a); }
    SimdFloat operator^(const SimdFloat &o) const { return _mm_xor_ps(_a, o._a); }

    SimdFloat operator&(const SimdBool<4> &o) const { return _mm_and_ps(_a, o._b); }
    SimdFloat blend(const SimdFloat &o, const SimdBool<4> &mask) const
    {
#ifdef __SSE4_1__
        return _mm_blendv_ps(_a, o._a, mask._b);
#else
        return _mm_or_ps(_mm_and_ps(o._a, mask._b), _mm_andnot_ps(mask._b, _a));
#endif
    }

    SimdFloat &operator+=(const SimdFloat &o) { _a = _mm_add_ps(_a, o._a); return *this; }
    SimdFloat &operator-=(const SimdFloat &o) { _a = _mm_sub_ps(_a, o._a); return *this; }
    SimdFloat &operator*=(const SimdFloat &o) { _a = _mm_mul_ps(_a, o._a); return *this; }
    SimdFloat &operator/=(const SimdFloat &o) { _a = _mm_div_ps(_a, o._a); return *this; }
    SimdFloat &operator^=(const SimdFloat &o) { _a = _mm_xor_ps(_a, o._a); return *this; }

    SimdBool<4> operator==(const SimdFloat &o) const { return SimdBool<4>(_mm_cmpeq_ps (_a, o._a)); }
    SimdBool<4> operator!=(const SimdFloat &o) const { return SimdBool<4>(_mm_cmpneq_ps(_a, o._a)); }
    SimdBool<4> operator<=(const SimdFloat &o) const { return SimdBool<4>(_mm_cmple_ps (_a, o._a)); }
    SimdBool<4> operator>=(const SimdFloat &o) const { return SimdBool<4>(_mm_cmpge_ps (_a, o._a)); }
    SimdBool<4> operator< (const SimdFloat &o) const { return SimdBool<4>(_mm_cmplt_ps (_a, o._a)); }
    SimdBool<4> operator> (const SimdFloat &o) const { return SimdBool<4>(_mm_cmpgt_ps (_a, o._a)); }

    float *data() { return _f; }
    const float *data() const { return _f; };

    const __m128 &raw() const { return _a; }

    float sum() const {
#ifdef __SSE3__
        __m128 tmp = _mm_hadd_ps(_a, _a);
        return _mm_cvtss_f32(_mm_hadd_ps(tmp, tmp));
#else
        alignas(16) float as[4];
        _mm_store_ps(as, _a);
        return as[0] + as[1] + as[2] + as[3];
#endif
    }

    float &operator[](unsigned i) { return _f[i]; }
    float operator[](unsigned i) const { return _f[i]; }
};

typedef SimdFloat<4> float4;

#endif

#ifdef __AVX__
template<>
class SimdFloat<8>
{
    union {
        __m256 _a;
        float _f[8];
        uint32 _i[8];
    };

    SimdFloat(const __m256 &a)
    : _a(a)
    {
    }

    friend SimdFloat<8> min(const Tungsten::SimdFloat<8> &, const Tungsten::SimdFloat<8> &);
    friend SimdFloat<8> max(const Tungsten::SimdFloat<8> &, const Tungsten::SimdFloat<8> &);
    friend SimdFloat<8> sqrt(const Tungsten::SimdFloat<8> &);

public:
    static CONSTEXPR uint32 n = 8;
    static CONSTEXPR size_t Alignment = 8*sizeof(float);

    SimdFloat() = default;
    SimdFloat(const SimdFloat &o) = default;
    SimdFloat(const float *a)
    : _a(_mm256_load_ps(a))
    {
    }
    SimdFloat(float a)
    : _a(_mm256_set1_ps(a))
    {
    }

    SimdFloat operator-() const { return _mm256_sub_ps(_mm256_setzero_ps(), _a); }
    SimdFloat operator+(const SimdFloat &o) const { return _mm256_add_ps(_a, o._a); }
    SimdFloat operator-(const SimdFloat &o) const { return _mm256_sub_ps(_a, o._a); }
    SimdFloat operator*(const SimdFloat &o) const { return _mm256_mul_ps(_a, o._a); }
    SimdFloat operator/(const SimdFloat &o) const { return _mm256_div_ps(_a, o._a); }

    SimdFloat operator&(const SimdBool<8> &o) const { return _mm256_and_ps(_a, o._b); }
    SimdFloat blend(const SimdFloat &o, const SimdBool<8> &mask) const { return _mm256_blendv_ps(_a, o._a, mask._b); }

    SimdFloat &operator+=(const SimdFloat &o) { _a = _mm256_add_ps(_a, o._a); return *this; }
    SimdFloat &operator-=(const SimdFloat &o) { _a = _mm256_sub_ps(_a, o._a); return *this; }
    SimdFloat &operator*=(const SimdFloat &o) { _a = _mm256_mul_ps(_a, o._a); return *this; }
    SimdFloat &operator/=(const SimdFloat &o) { _a = _mm256_div_ps(_a, o._a); return *this; }

    SimdBool<8> operator==(const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_EQ_OQ)); }
    SimdBool<8> operator!=(const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_NEQ_OQ)); }
    SimdBool<8> operator<=(const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_LE_OQ)); }
    SimdBool<8> operator>=(const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_GE_OQ)); }
    SimdBool<8> operator< (const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_LT_OQ)); }
    SimdBool<8> operator> (const SimdFloat &o) const { return SimdBool<8>(_mm256_cmp_ps(_a, o._a, _CMP_GT_OQ)); }

    float *data() { return _f; }
    const float *data() const { return _f; };

    float sum() const {
        alignas(32) float as[8];
        _mm256_store_ps(as, _a);
        return as[0] + as[1] + as[2] + as[3] + as[4] + as[5] + as[6] + as[7];
    }

    float &operator[](unsigned i) { return _f[i]; }
    float operator[](unsigned i) const { return _f[i]; }
};

typedef SimdFloat<4> float8;

#endif

inline SimdFloat<1> min(const SimdFloat<1> &a, const SimdFloat<1> &b)
{
    return a._a < b._a ? a : b;
}

inline SimdFloat<1> max(const SimdFloat<1> &a, const SimdFloat<1> &b)
{
    return a._a > b._a ? a : b;
}

#ifdef __SSE__
inline SimdFloat<4> min(const SimdFloat<4> &a, const SimdFloat<4> &b)
{
    return SimdFloat<4>(_mm_min_ps(a._a, b._a));
}

inline SimdFloat<4> max(const SimdFloat<4> &a, const SimdFloat<4> &b)
{
    return SimdFloat<4>(_mm_max_ps(a._a, b._a));
}
#endif

#ifdef __AVX__
inline SimdFloat<8> min(const SimdFloat<8> &a, const SimdFloat<8> &b)
{
    return SimdFloat<8>(_mm256_min_ps(a._a, b._a));
}

inline SimdFloat<8> max(const SimdFloat<8> &a, const SimdFloat<8> &b)
{
    return SimdFloat<8>(_mm256_max_ps(a._a, b._a));
}
#endif

inline Tungsten::SimdFloat<1> sqrt(const Tungsten::SimdFloat<1> &a)
{
    return Tungsten::SimdFloat<1>(std::sqrt(a._a));
}

#ifdef __SSE__
inline Tungsten::SimdFloat<4> sqrt(const Tungsten::SimdFloat<4> &a)
{
    return Tungsten::SimdFloat<4>(_mm_sqrt_ps(a._a));
}
#endif

#ifdef __AVX__
inline Tungsten::SimdFloat<8> sqrt(const Tungsten::SimdFloat<8> &a)
{
    return Tungsten::SimdFloat<8>(_mm256_sqrt_ps(a._a));
}
#endif


}


#endif /* SIMDFLOAT_HPP_ */
