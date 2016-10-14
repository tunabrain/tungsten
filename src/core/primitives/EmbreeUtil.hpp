#ifndef EMBREEUTIL_HPP_
#define EMBREEUTIL_HPP_

#include "math/Ray.hpp"
#include "math/Box.hpp"
#include "math/Mat4f.hpp"

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

namespace Tungsten {

namespace EmbreeUtil {

void initDevice();
RTCDevice getDevice();

inline RTCBounds convert(const Box3f &b)
{
    return RTCBounds{
        b.min().x(), b.min().y(), b.min().z(), 0.0f,
        b.max().x(), b.max().y(), b.max().z(), 0.0f
    };
}

inline Box3f convert(const RTCBounds &b)
{
    return Box3f(
        Vec3f(b.lower_x, b.lower_y, b.lower_z),
        Vec3f(b.upper_x, b.upper_y, b.upper_z)
    );
}

inline Ray convert(const RTCRay &r)
{
    return Ray(Vec3f(r.org), Vec3f(r.dir), r.tnear, r.tfar);
}

inline RTCRay convert(const Ray &r)
{
    RTCRay ray;
    ray.org[0] = r.pos().x();
    ray.org[1] = r.pos().y();
    ray.org[2] = r.pos().z();
    ray.dir[0] = r.dir().x();
    ray.dir[1] = r.dir().y();
    ray.dir[2] = r.dir().z();
    ray.tnear = r.nearT();
    ray.tfar  = r.farT();
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
    ray.primID = RTC_INVALID_GEOMETRY_ID;
    return ray;
}

}

}

#endif /* EMBREEUTIL_HPP_ */
