#ifndef SIMDVEC_HPP_
#define SIMDVEC_HPP_

#include "SimdFloat.hpp"

namespace Tungsten {

// TODO: Implement proper vector functionality
typedef float4 Vec4fp;
typedef float4 Vec3fp;
typedef float4 Vec2fp;

inline Vec3fp expand(const Vec3f &a)
{
    return Vec3fp(a.x(), a.y(), a.z(), 0.0f);
}

inline Vec3f narrow(const Vec3fp &a)
{
    return Vec3f(a[0], a[1], a[2]);
}

}

#endif /* SIMDVEC_HPP_ */
