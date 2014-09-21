#ifndef BVH_PRIMITIVE_HPP_
#define BVH_PRIMITIVE_HPP_

#include "math/Vec.hpp"
#include "math/Box.hpp"

#include "AlignedAllocator.hpp"

#include <vector>

namespace Tungsten {

namespace Bvh {

class Primitive
{
    Box3fp _box;
    Vec3fp _centroid;
    uint32 _id;

    float _area;
public:
    Primitive(const Box3f &box, const Vec3f &centroid, uint32 id)
    : _box(expand(box)), _centroid(expand(centroid)), _id(id), _area(box.area())
    {
    }

    Primitive(const Vec3f &p0, const Vec3f &p1, const Vec3f &p2, uint32 id)
    : _centroid(expand((p0 + p1 + p2)/3.0f)),
      _id(id)
    {
        _box.grow(expand(p0));
        _box.grow(expand(p1));
        _box.grow(expand(p2));
        _area = _box.area();
    }

    float area() const
    {
        return _area;
    }

    void setArea(float area)
    {
        _area = area;
    }

    const Box3fp &box() const
    {
        return _box;
    }

    const Vec3fp &centroid() const
    {
        return _centroid;
    }

    uint32 id() const
    {
        return _id;
    }
};

typedef std::vector<Primitive, AlignedAllocator<Primitive, 16>> PrimVector;

}

}

#endif /* BVH_PRIMITIVE_HPP_ */
