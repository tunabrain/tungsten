#ifndef TANGENTSPACE_HPP_
#define TANGENTSPACE_HPP_

#include <cmath>

#include "Vec.hpp"

namespace Tungsten
{

struct TangentSpace
{
    Vec3f normal, tangent, bitangent;

    TangentSpace(const Vec3f &n)
    : normal(n)
    {
        if (std::abs(normal.x()) > std::abs(normal.y()))
            tangent = Vec3f(0.0f, 1.0f, 0.0f);
        else
            tangent = Vec3f(1.0f, 0.0f, 0.0f);
        bitangent = normal.cross(tangent).normalized();
        tangent = bitangent.cross(normal);
    }

    Vec3f toLocal(const Vec3f &p) const
    {
        return Vec3f(
            tangent.dot(p),
            bitangent.dot(p),
            normal.dot(p)
        );
    }

    Vec3f toGlobal(const Vec3f &p) const
    {
        return tangent*p.x() + bitangent*p.y() + normal*p.z();
    }
};

}

#endif /* TANGENTSPACE_HPP_ */
