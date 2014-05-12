#ifndef MATHUTIL_HPP_
#define MATHUTIL_HPP_

#include "Vec.hpp"

#include "IntTypes.hpp"

namespace Tungsten
{

template<typename T>
T min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T, typename... Ts>
T min(T a, T b, Ts... ts)
{
    return min(min(a, b), ts...);
}

template<typename T>
T max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T, typename... Ts>
T max(T a, T b, Ts... ts)
{
    return max(max(a, b), ts...);
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> min(const Vec<ElementType, Size> &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result(a);
    for (unsigned i = 0; i < Size; ++i)
        if (b.data()[i] < a.data()[i])
            result.data()[i] = b.data()[i];
    return result;
}

template<typename ElementType, unsigned Size>
Vec<ElementType, Size> max(const Vec<ElementType, Size> &a, const Vec<ElementType, Size> &b)
{
    Vec<ElementType, Size> result(a);
    for (unsigned i = 0; i < Size; ++i)
        if (b.data()[i] > a.data()[i])
            result.data()[i] = b.data()[i];
    return result;
}

template<typename T>
T clamp(T val, T minVal, T maxVal)
{
    return min(max(val, minVal), maxVal);
}

template<typename T>
T sqr(T val)
{
    return val*val;
}

template<typename T>
T cube(T val)
{
    return val*val*val;
}

template<typename T>
T smoothStep(T edge0, T edge1, T x) {
    x = clamp((x - edge0)/(edge1 - edge0), T(0), T(1));

    return x*x*(T(3) - T(2)*x);
}

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

template <typename T> T sgnE(T val) {
    return val < T(0) ? T(-1) : T(1);
}

class MathUtil
{
public:
    static inline uint32 hash32(uint32 x) {
        x = ~x + (x << 15);
        x = x ^ (x >> 12);
        x = x + (x << 2);
        x = x ^ (x >> 4);
        x = x * 2057;
        x = x ^ (x >> 16);
        return x;
    }

    static float sphericalDistance(float lat0, float long0, float lat1, float long1, float r)
    {
        float  latSin = std::sin(( lat1 -  lat0)*0.5f);
        float longSin = std::sin((long1 - long0)*0.5f);
        return 2.0f*r*std::asin(std::sqrt(latSin*latSin + std::cos(lat0)*std::cos(lat1)*longSin*longSin));
    }

    // See http://geomalgorithms.com/a07-_distance.html
    static Vec2f closestPointBetweenLines(const Vec3f& P0, const Vec3f& u, const Vec3f& Q0, const Vec3f& v)
    {
        const Vec3f w0 = P0 - Q0;
        const float a = u.dot(u);
        const float b = u.dot(v);
        const float c = v.dot(v);
        const float d = u.dot(w0);
        const float e = v.dot(w0);
        float denom = a*c - b*b;
        if (denom == 0.0f)
            return Vec2f(0.0f);
        else
            return Vec2f(b*e - c*d, a*e - b*d)/denom;
    }

    static float triangleArea(const Vec3f &a, const Vec3f &b, const Vec3f &c)
    {
        return (b - a).cross(c - a).length()*0.5f;
    }
};

}

#endif /* MATHUTIL_HPP_ */
