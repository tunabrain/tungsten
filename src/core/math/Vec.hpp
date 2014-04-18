#ifndef VEC_HPP_
#define VEC_HPP_

#include <rapidjson/document.h>
#include <ostream>
#include <cmath>

#include "math/BitManip.hpp"

#include "IntTypes.hpp"

namespace Tungsten
{

template<typename ElementType, unsigned Size>
class Vec {
    typedef Vec<ElementType, Size> TVec;

    ElementType _v[Size];

public:
    static const unsigned size = Size;
    typedef ElementType elementType;

    Vec()
    : _v{ElementType(0)}
    {
    }

    explicit Vec(ElementType a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] = a;
    }

    template<typename... Ts>
    explicit Vec(Ts... ts)
    : _v{ts...}
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

    TVec normalized() const
    {
        ElementType invLen = ElementType(1)/length();
        TVec other(*this);
        for (unsigned i = 0; i < Size; ++i)
            other._v[i] *= invLen;
        return other;
    }

    ElementType dot(const TVec &other) const
    {
        ElementType sum = _v[0]*other._v[0];
        for (unsigned i = 1; i < Size; ++i)
            sum += _v[i]*other._v[i];
        return sum;
    }

    TVec cross(const TVec &other) const
    {
        static_assert(Size == 3, "Cross product only defined in three dimensions!");
        return TVec(
            y()*other.z() - z()*other.y(),
            z()*other.x() - x()*other.z(),
            x()*other.y() - y()*other.x()
        );
    }

    ElementType &operator[](unsigned i)
    {
        return _v[i];
    }

    const ElementType &operator[](unsigned i) const
    {
        return _v[i];
    }

    TVec operator-() const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = -_v[i];
        return result;
    }

    TVec operator+(const TVec &other) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] + other._v[i];
        return result;
    }

    TVec operator-(const TVec &other) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] - other._v[i];
        return result;
    }

    TVec operator*(const TVec &other) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]*other._v[i];
        return result;
    }

    TVec operator/(const TVec &other) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]/other._v[i];
        return result;
    }

    TVec operator+(ElementType a) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] + a;
        return result;
    }

    TVec operator-(ElementType a) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i] - a;
        return result;
    }

    TVec operator*(ElementType a) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]*a;
        return result;
    }

    TVec operator/(ElementType a) const
    {
        TVec result;
        for (unsigned i = 0; i < Size; ++i)
            result._v[i] = _v[i]/a;
        return result;
    }

    TVec operator+=(const TVec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] += other._v[i];
        return *this;
    }

    TVec operator-=(const TVec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] -= other._v[i];
        return *this;
    }

    TVec operator*=(const TVec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] *= other._v[i];
        return *this;
    }

    TVec operator/=(const TVec &other)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] /= other._v[i];
        return *this;
    }

    TVec operator+=(ElementType a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] += a;
        return *this;
    }

    TVec operator-=(ElementType a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] -= a;
        return *this;
    }

    TVec operator*=(ElementType a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] *= a;
        return *this;
    }

    TVec operator/=(ElementType a)
    {
        for (unsigned i = 0; i < Size; ++i)
            _v[i] /= a;
        return *this;
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

    bool operator==(const TVec &o) const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_v[i] != o[i])
                return false;
        return true;
    }

    bool operator!=(const TVec &o) const
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
        for (uint32 i = 0; i < v.size; ++i)
            stream << v[i] << (i == v.size - 1 ? ')' : ',');
        return stream;
    }
};

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator+(ElementType a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a + b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator-(ElementType a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a - b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator*(ElementType a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result[i] = a*b[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> operator/(ElementType a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result;
    for (unsigned i = 0; i < Size; ++i)
        result._v[i] = a/b._v[i];
    return result;
}

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

}

namespace std {

template<typename ElementType, unsigned Size>
class hash<Tungsten::Vec<ElementType, Size>>
{
public:
    std::size_t operator()(const Tungsten::Vec<ElementType, Size> &v) const {
        // See http://www.boost.org/doc/libs/1_33_1/doc/html/hash_combine.html
        Tungsten::uint32 result = 0;
        for (unsigned i = 0; i < Size; ++i)
            result ^= Tungsten::BitManip::floatBitsToUint(v[i]) + 0x9E3779B9 + (result << 6) + (result >> 2);
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

}

#endif /* VEC_HPP_ */
