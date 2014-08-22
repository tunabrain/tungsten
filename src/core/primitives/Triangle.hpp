#ifndef TRIANGLE_HPP_
#define TRIANGLE_HPP_

#include "Vertex.hpp"

#include "math/Vec.hpp"

#include "IntTypes.hpp"

namespace Tungsten {

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
