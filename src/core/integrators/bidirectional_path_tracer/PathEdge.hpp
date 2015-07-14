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
    float pdfForward;
    float pdfBackward;

    PathEdge() = default;
    PathEdge(const Vec3f &d_, float r_, float rSq_)
    : PathEdge(d_, r_, rSq_, 1.0f, 1.0f)
    {
    }
    PathEdge(const Vec3f &d_, float r_, float rSq_, float pdfForward_, float pdfBackward_)
    : d(d_),
      r(r_),
      rSq(rSq_),
      pdfForward(pdfForward_),
      pdfBackward(pdfBackward_)
    {
    }
    PathEdge(const PathVertex &root, const PathVertex &tip)
    : PathEdge(root, tip, 1.0f, 1.0f)
    {
    }
    PathEdge(const PathVertex &root, const PathVertex &tip, float pdfForward_, float pdfBackward_)
    {
        d = tip.pos() - root.pos();
        rSq = d.lengthSq();
        r = std::sqrt(rSq);
        if (r != 0.0f)
            d *= 1.0f/r;
        pdfForward = pdfForward_;
        pdfBackward = pdfBackward_;
    }

    PathEdge reverse() const
    {
        return PathEdge(-d, r, rSq, pdfBackward, pdfForward);
    }
};

}

#endif /* PATHEDGE_HPP_ */
