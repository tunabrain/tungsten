#ifndef TRIANGLE_HPP_
#define TRIANGLE_HPP_

#include "Vertex.hpp"

#include "math/Vec.hpp"

#include "IntTypes.hpp"

namespace Tungsten {

class Triangle
{
    Vertex _v0, _v1, _v2;
    int32 _material;
    uint16 _space0;
    uint16 _space1;

public:
    Triangle() = default;

    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2, uint32 material, uint16 space0, uint16 space1)
    : _v0(v0), _v1(v1), _v2(v2), _material(material), _space0(space0), _space1(space1)
    {
    }

    Vec2f uvAt(Vec2f lambda) const
    {
        return (1.0f - lambda.x() - lambda.y())*_v0.uv()
               + lambda.x()*_v1.uv()
               + lambda.y()*_v2.uv();
    }

    Vec3f normalAt(Vec2f lambda) const
    {
        return ((1.0f - lambda.x() - lambda.y())*_v0.normal()
               + lambda.x()*_v1.normal()
               + lambda.y()*_v2.normal()).normalized();
    }

    int32 material() const
    {
        return _material;
    }

    uint16 other(uint16 space) const
    {
        return _space0 == space ? _space1 : _space0;
    }
};

struct TriangleI
{
    union {
        struct { uint32 v0, v1, v2; };
        uint32 vs[3];
    };
    int32 material;

    TriangleI() = default;

    TriangleI(uint32 v0_, uint32 v1_, uint32 v2_, int32 material_ = -1)
    : v0(v0_), v1(v1_), v2(v2_), material(material_)
    {
    }
};

}


#endif /* TRIANGLE_HPP_ */
