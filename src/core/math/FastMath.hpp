#ifndef FASTMATH_HPP_
#define FASTMATH_HPP_

#include "Vec.hpp"

#include "sse/SimdFloat.hpp"

#include <fmath/fmath.hpp>

namespace Tungsten {

namespace FastMath {

static inline float exp(float f)
{
    return fmath::exp(f);
}
static inline Vec2f exp(Vec2f f)
{
    float4 result = fmath::exp_ps(float4(f[0], f[1], 0.0f, 0.0f).raw());
    return Vec2f(result[0], result[1]);
}
static inline Vec3f exp(Vec3f f)
{
    float4 result = fmath::exp_ps(float4(f[0], f[1], f[2], 0.0f).raw());
    return Vec3f(result[0], result[1], result[2]);
}
static inline Vec4f exp(Vec4f f)
{
    float4 result = fmath::exp_ps(float4(f[0], f[1], f[2], f[3]).raw());
    return Vec4f(result[0], result[1], result[2], result[3]);
}

static inline float log(float f)
{
    return fmath::log(f);
}
static inline Vec2f log(Vec2f f)
{
    float4 result = fmath::log_ps(float4(f[0], f[1], 0.0f, 0.0f).raw());
    return Vec2f(result[0], result[1]);
}
static inline Vec3f log(Vec3f f)
{
    float4 result = fmath::log_ps(float4(f[0], f[1], f[2], 0.0f).raw());
    return Vec3f(result[0], result[1], result[2]);
}
static inline Vec4f log(Vec4f f)
{
    float4 result = fmath::log_ps(float4(f[0], f[1], f[2], f[3]).raw());
    return Vec4f(result[0], result[1], result[2], result[3]);
}

}

}



#endif /* FASTMATH_HPP_ */
