#ifndef BSPLINE_HPP_
#define BSPLINE_HPP_

namespace Tungsten {

namespace BSpline {

// http://www.answers.com/topic/b-spline
template<typename T>
inline T quadratic(T p0, T p1, T p2, float t)
{
    return (0.5f*p0 - p1 + 0.5f*p2)*t*t + (p1 - p0)*t + 0.5f*(p0 + p1);
}

template<typename T>
inline T quadraticDeriv(T p0, T p1, T p2, float t)
{
    return (p0 - 2.0f*p1 + p2)*t + (p1 - p0);
}

inline Vec2f quadraticMinMax(float p0, float p1, float p2)
{
    float xMin = (p0 + p1)*0.5f;
    float xMax = (p1 + p2)*0.5f;
    if (xMin > xMax)
        std::swap(xMin, xMax);

    float tFlat = (p0 - p1)/(p0 - 2.0f*p1 + p2);
    if (tFlat > 0.0f && tFlat < 1.0f) {
        float xFlat = quadratic(p0, p1, p2, tFlat);
        xMin = min(xMin, xFlat);
        xMax = max(xMax, xFlat);
    }
    return Vec2f(xMin, xMax);
}

}

}

#endif /* BSPLINE_HPP_ */
