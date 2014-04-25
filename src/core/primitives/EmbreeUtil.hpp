#ifndef EMBREEUTIL_HPP_
#define EMBREEUTIL_HPP_

#include "math/Ray.hpp"

#include <embree/common/ray.h>

namespace Tungsten {

inline embree::Vec3fa toE(const Vec3f &v)
{
    return embree::Vec3fa(v.x(), v.y(), v.z());
}

inline Vec3f fromE(const embree::Vec3fa v)
{
    return Vec3f(v.x, v.y, v.z);
}

inline embree::BBox3f toEBox(const Box3f &b)
{
    return embree::BBox3f(toE(b.min()), toE(b.max()));
}

inline Box3f fromEBox(const embree::BBox3f &b)
{
    return Box3f(fromE(b.lower), fromE(b.upper));
}

inline Ray fromERay(const embree::Ray &r)
{
    return Ray(fromE(r.org), fromE(r.dir), r.tnear, r.tfar);//, r.time);
}

inline embree::Ray toERay(const Ray &r)
{
    return embree::Ray(toE(r.pos()), toE(r.dir()), r.nearT(), r.farT());//, r.time(), r.flags());
}

}

#endif /* EMBREEUTIL_HPP_ */
