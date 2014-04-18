#ifndef SAMPLE_HPP_
#define SAMPLE_HPP_

#include <cmath>

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

namespace Tungsten
{

static inline Vec3f uniformHemisphere(const Vec2f &uv)
{
    float phi  = TWO_PI*uv.x();
    float invU = std::sqrt(max(1.0f - sqr(uv.y()), 0.0f));
    return Vec3f(std::cos(phi)*invU, std::sin(phi)*invU, uv.y());
}

static inline float uniformHemispherePdf(const Vec3f &/*p*/)
{
    return INV_TWO_PI;
}

static inline Vec3f cosineHemisphere(const Vec2f &uv)
{
    float phi = uv.x()*TWO_PI;
    float r = std::sqrt(uv.y());

    return Vec3f(std::cos(phi)*r, std::sin(phi)*r, std::sqrt(max(1.0f - uv.y(), 0.0f)));
}

static inline float cosineHemispherePdf(const Vec3f &p)
{
    return p.z()*INV_PI;
}

}

#endif /* SAMPLE_HPP_ */
