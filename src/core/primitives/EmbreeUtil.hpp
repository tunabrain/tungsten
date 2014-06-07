#ifndef EMBREEUTIL_HPP_
#define EMBREEUTIL_HPP_

#include "math/Ray.hpp"
#include "math/Box.hpp"
#include "math/Mat4f.hpp"

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

inline embree::AffineSpace3f toEMat(const Mat4f &m)
{
    return embree::AffineSpace3f(
        embree::LinearSpace3f(
            embree::Vector3f(m[0], m[4], m[ 8]),
            embree::Vector3f(m[1], m[5], m[ 9]),
            embree::Vector3f(m[2], m[6], m[10])
        ),
        embree::Vector3f(m[3], m[7], m[11])
    );
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
