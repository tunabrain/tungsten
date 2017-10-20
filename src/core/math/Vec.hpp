#ifndef VEC_HPP_
#define VEC_HPP_

#include "math/BitManip.hpp"

#include "IntTypes.hpp"

#include <rapidjson/document.h>
#include <type_traits>
#include <ostream>
#include <array>
#include <cmath>

namespace Tungsten {

template<typename ElementType, unsigned Size>
class Vec {
protected:
    std::array<ElementType, Size> _v;

public:
    static const unsigned size = Size;

    Vec() = default;

    explicit Vec(const ElementType &a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] = a;
    }

    explicit Vec(const ElementType *a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] = a[i];
    }

    template<typename... Ts>
    Vec(const ElementType &a, const ElementType &b, const Ts &... ts)
    : _v({{a, b, ts...}})
    {
    }

    template<typename OtherType>
    explicit Vec(const Vec<OtherType, Size> &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] = ElementType(other[i]);
    }

    ElementType x() const
    {
        return _v[0];
    }

    ElementType y() const
    {
        static_assert(Size > 1, "Vector does not have y coordinate");
        return _v[1];
    }

    ElementType z() const
    {
        static_assert(Size > 2, "Vector does not have z coordinate");
        return _v[2];
    }

    ElementType w() const
    {
        static_assert(Size > 3, "Vector does not have w coordinate");
        return _v[3];
    }

    ElementType &x()
    {
        return _v[0];
    }

    ElementType &y()
    {
        static_assert(Size > 1, "Vector does not have y coordinate");
        return _v[1];
    }

    ElementType &z()
    {
        static_assert(Size > 2, "Vector does not have z coordinate");
        return _v[2];
    }

    ElementType &w()
    {
        static_assert(Size > 3, "Vector does not have w coordinate");
        return _v[3];
    }

    Vec<ElementType, 2> xy() const
    {
        return Vec<ElementType, 2>(x(), y());
    }

    Vec<ElementType, 3> xyz() const
    {
        return Vec<ElementType, 3>(x(), y(), z());
    }

    Vec<ElementType, 2> xz() const
    {
        return Vec<ElementType, 2>(x(), z());
    }

    Vec<ElementType, 2> yz() const
    {
        return Vec<ElementType, 2>(y(), z());
    }

    ElementType *data()
    {
        return &_v[0];
    }

    const ElementType *data() const
    {
        return &_v[0];
    }

    ElementType lengthSq() const
    {
        ElementType res = ElementType(0);
        for (unsigned i = 0; i < Size; ++i)
            res += _v[i]*_v[i];
        return res;
    }

    ElementType length() const
    {
        return std::sqrt(lengthSq());
    }

    ElementType sum() const
    {
        ElementType result(_v[0]);
        for (unsigned i = 1; i < Size; ++i)
            result += _v[i];
        return result;
    }

    ElementType avg() const
    {
        return sum()*(ElementType(1)/ElementType(Size));
    }

    ElementType product() const
    {
        ElementType result(_v[0]);
        for (unsigned i = 1; i < Size; ++i)
            result *= _v[i];
        return result;
    }

    void normalize()
    {
        ElementType invLen = ElementType(1)/length();
        for (unsigned i = 0; i < Size; ++i)
            _v[i] *= invLen;
    }

    Vec normalized() const
    {
        ElementType invLen = ElementType(1)/length();
        Vec other(*this);
        for (unsigned i = 0; i < Size; ++i)
            other._v[i] *= invLen;
        return other;
    }

    ElementType dot(const Vec &other) const
    {
        ElementType sum = _v[0]*other._v[0];
        for (unsigned i = 1; i < Size; ++i)
            sum += _v[i]*other._v[i];
        return sum;
    }

    Vec cross(const Vec &other) const
    {
        static_assert(Size == 3, "Cross product only defined in three dimensions!");
        return Vec(
            y()*other.z() - z()*other.y(),
            z()*other.x() - x()*other.z(),
            x()*other.y() - y()*other.x()
        );
    }

    ElementType luminance() const
    {
        static_assert(Size == 3, "Luminance only supported in three dimensions!");
        return x()*ElementType(0.2126) + y()*ElementType(0.7152) + z()*ElementType(0.0722);
    }

    ElementType &operator[](unsigned i)
    {
        return _v[i];
    }

    const ElementType &operator[](unsigned i) const
    {
        return _v[i];
    }

    Vec operator-() const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = -_v[i];
        return result;
    }

    Vec operator+(const Vec &other) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] + other._v[i];
        return result;
    }

    Vec operator-(const Vec &other) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] - other._v[i];
        return result;
    }

    Vec operator*(const Vec &other) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]*other._v[i];
        return result;
    }

    Vec operator/(const Vec &other) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]/other._v[i];
        return result;
    }

    Vec operator+(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] + a;
        return result;
    }

    Vec operator-(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] - a;
        return result;
    }

    Vec operator*(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]*a;
        return result;
    }

    Vec operator/(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]/a;
        return result;
    }

    Vec operator>>(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] >> a;
        return result;
    }

    Vec operator<<(const ElementType &a) const
    {
        Vec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] << a;
        return result;
    }

    Vec operator+=(const Vec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] += other._v[i];
        return *this;
    }

    Vec operator-=(const Vec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] -= other._v[i];
        return *this;
    }

    Vec operator*=(const Vec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] *= other._v[i];
        return *this;
    }

    Vec operator/=(const Vec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] /= other._v[i];
        return *this;
    }

    Vec operator+=(const ElementType &a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] += a;
        return *this;
    }

    Vec operator-=(const ElementType &a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] -= a;
        return *this;
    }

    Vec operator*=(const ElementType &a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] *= a;
        return *this;
    }

    Vec operator/=(const ElementType &a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] /= a;
        return *this;
    }

    Vec operator>>=(const ElementType &a) const
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] >>= a;
        return *this;
    }

    Vec operator<<=(const ElementType &a) const
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] <<= a;
        return *this;
    }

    uint32 maxDim() const
    {
        ElementType m = _v[0];
        uint32 idx = 0;
        for (unsigned i = 1; i < Size; ++i) {
            if (_v[i] > m) {
                m = _v[i];
                idx = i;
            }
        }
        return idx;
    }

    uint32 minDim() const
    {
        ElementType m = _v[0];
        uint32 idx = 0;
        for (unsigned i = 1; i < Size; ++i) {
            if (_v[i] < m) {
                m = _v[i];
                idx = i;
            }
        }
        return idx;
    }

    ElementType max() const
    {
        ElementType m = _v[0];
        for (unsigned i = 1; i < Size; ++i)
            if (_v[i] > m)
                m = _v[i];
        return m;
    }

    ElementType min() const
    {
        ElementType m = _v[0];
        for (unsigned i = 1; i < Size; ++i)
            if (_v[i] < m)
                m = _v[i];
        return m;
    }

    bool operator==(const Vec &o) const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_v[i] != o[i])
                return false;
        return true;
    }

    bool operator!=(const Vec &o) const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_v[i] != o[i])
                return true;
        return false;
    }

    bool operator==(const ElementType &a) const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_v[i] != a)
                return false;
        return true;
    }

    bool operator!=(const ElementType &a) const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_v[i] != a)
                return true;
        return false;
    }

    friend std::ostream &operator<< (std::ostream &stream, const Vec &v) {
        stream << '(';
        for (uint32 i = 0; i < Size; ++i)
            stream << v[i] << (i == Size - 1 ? ')' : ',');
        return stream;
    }
};

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator+(const ElementType &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a + b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator-(const ElementType &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a - b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator*(const ElementType &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a*b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator/(const ElementType &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a/b[i];
    return result;
}

typedef Vec<double, 4> Vec4d;
typedef Vec<double, 3> Vec3d;
typedef Vec<double, 2> Vec2d;

typedef Vec<float, 4> Vec4f;
typedef Vec<float, 3> Vec3f;
typedef Vec<float, 2> Vec2f;

typedef Vec<uint32, 4> Vec4u;
typedef Vec<uint32, 3> Vec3u;
typedef Vec<uint32, 2> Vec2u;

typedef Vec<int32, 4> Vec4i;
typedef Vec<int32, 3> Vec3i;
typedef Vec<int32, 2> Vec2i;

typedef Vec<uint8, 4> Vec4c;
typedef Vec<uint8, 3> Vec3c;
typedef Vec<uint8, 2> Vec2c;

// Vec classes were accidentally turned into not-POD-types on several occasions,
// but a lot of code relies on vectors being POD. The static_asserts here catch
// it in case another mistake happens

// MSVC's views on what is POD or not differ from gcc or clang.
// memcpy and similar code still seem to work, so we ignore this
// issue for now.
#ifndef _MSC_VER
static_assert(std::is_pod<Vec4d>::value, "Vec4d is not a pod!");
static_assert(std::is_pod<Vec3d>::value, "Vec3d is not a pod!");
static_assert(std::is_pod<Vec2d>::value, "Vec2d is not a pod!");

static_assert(std::is_pod<Vec4f>::value, "Vec4f is not a pod!");
static_assert(std::is_pod<Vec3f>::value, "Vec3f is not a pod!");
static_assert(std::is_pod<Vec2f>::value, "Vec2f is not a pod!");

static_assert(std::is_pod<Vec4u>::value, "Vec4u is not a pod!");
static_assert(std::is_pod<Vec3u>::value, "Vec3u is not a pod!");
static_assert(std::is_pod<Vec2u>::value, "Vec2u is not a pod!");

static_assert(std::is_pod<Vec4i>::value, "Vec4i is not a pod!");
static_assert(std::is_pod<Vec3i>::value, "Vec3i is not a pod!");
static_assert(std::is_pod<Vec2i>::value, "Vec2i is not a pod!");

static_assert(std::is_pod<Vec4c>::value, "Vec4c is not a pod!");
static_assert(std::is_pod<Vec3c>::value, "Vec3c is not a pod!");
static_assert(std::is_pod<Vec2c>::value, "Vec2c is not a pod!");
#endif

}

namespace std {

template<unsigned Size>
class hash<Tungsten::Vec<float, Size>>
{
public:
    std::size_t operator()(const Tungsten::Vec<float, Size> &v) const {
        // See http://www.boost.org/doc/libs/1_33_1/doc/html/hash_combine.html
        Tungsten::uint32 result = 0;
        for (unsigned i = 0; i < Size; ++i)
            result ^= Tungsten::BitManip::floatBitsToUint(v[i]) + 0x9E3779B9 + (result << 6) + (result >> 2);
        return result;
    }
};

template<unsigned Size>
class hash<Tungsten::Vec<Tungsten::uint32, Size>>
{
public:
    std::size_t operator()(const Tungsten::Vec<Tungsten::uint32, Size> &v) const {
        // See http://www.boost.org/doc/libs/1_33_1/doc/html/hash_combine.html
        Tungsten::uint32 result = 0;
        for (unsigned i = 0; i < Size; ++i)
            result ^= v[i] + 0x9E3779B9 + (result << 6) + (result >> 2);
        return result;
    }
};

template<unsigned Size>
class hash<Tungsten::Vec<Tungsten::int32, Size>>
{
public:
    std::size_t operator()(const Tungsten::Vec<Tungsten::int32, Size> &v) const {
        // See http://www.boost.org/doc/libs/1_33_1/doc/html/hash_combine.html
        Tungsten::uint32 result = 0;
        for (unsigned i = 0; i < Size; ++i)
            result ^= static_cast<Tungsten::uint32>(v[i]) + 0x9E3779B9 + (result << 6) + (result >> 2);
        return result;
    }
};

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> exp(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::exp(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> sqrt(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::sqrt(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> pow(const Tungsten::Vec<ElementType, Size> &t, ElementType e)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::pow(t[i], e);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> pow(const Tungsten::Vec<ElementType, Size> &t, const Tungsten::Vec<ElementType, Size> &e)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::pow(t[i], e[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> log(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::log(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> abs(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::abs(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> floor(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::floor(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> ceil(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::ceil(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
Tungsten::Vec<ElementType, Size> trunc(const Tungsten::Vec<ElementType, Size> &t)
{
    Tungsten::Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = std::trunc(t[i]);
    return result;
}

template<typename ElementType, unsigned Size>
bool isnan(const Tungsten::Vec<ElementType, Size> &t)
{
    for (unsigned i = 0; i < Size; ++i)
        if (std::isnan(t[i]))
            return true;
    return false;
}

template<typename ElementType, unsigned Size>
bool isinf(const Tungsten::Vec<ElementType, Size> &t)
{
    for (unsigned i = 0; i < Size; ++i)
        if (std::isinf(t[i]))
            return true;
    return false;
}

}

#endif /* VEC_HPP_ */
