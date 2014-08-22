#ifndef EMBREEUTIL_HPP_
#define EMBREEUTIL_HPP_

#include "math/Ray.hpp"
#include "math/Box.hpp"
#include "math/Mat4f.hpp"

#include <embree/common/ray.h>

namespace Tungsten {

namespace EmbreeUtil {

inline embree::Vec3fa convert(const Vec3f &v)
{
    return embree::Vec3fa(v.x(), v.y(), v.z());
}

inline Vec3f convert(const embree::Vec3fa v)
{
    return Vec3f(v.x, v.y, v.z);
}

inline Vec3f convert(const embree::Vector3f v)
{
    return Vec3f(v.x, v.y, v.z);
}

inline embree::BBox3f convert(const Box3f &b)
{
    return embree::BBox3f(convert(b.min()), convert(b.max()));
}

inline embree::AffineSpace3f convert(const Mat4f &m)
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

inline Box3f convert(const embree::BBox3f &b)
{
    return Box3f(convert(b.lower), convert(b.upper));
}

inline Ray convert(const embree::Ray &r)
{
    return Ray(convert(r.org), convert(r.dir), r.tnear, r.tfar);//, r.time);
}

inline embree::Ray convert(const Ray &r)
{
    return embree::Ray(convert(r.pos()), convert(r.dir()), r.nearT(), r.farT());//, r.time(), r.flags());
}

}

}

#endif /* EMBREEUTIL_HPP_ */
