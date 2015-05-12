#ifndef PATHEDGE_HPP_
#define PATHEDGE_HPP_

#include "PathVertex.hpp"

#include "math/Vec.hpp"

namespace Tungsten {

struct PathEdge
{
    Vec3f d;
    float r;
    float rSq;

    PathEdge() = default;
    PathEdge(const Vec3f &d_, float r_, float rSq_)
    : d(d_),
      r(r_),
      rSq(rSq_)
    {
    }
    PathEdge(const PathVertex &root, const PathVertex &tip)
    {
        d = tip.pos() - root.pos();
        rSq = d.lengthSq();
        r = std::sqrt(rSq);
        if (r != 0.0f)
            d *= 1.0f/r;
    }

    PathEdge reverse() const
    {
        return PathEdge(-d, r, rSq);
    }
};

}

#endif /* PATHEDGE_HPP_ */
