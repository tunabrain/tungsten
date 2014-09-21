#ifndef MIDPOINTSPLITTER_HPP_
#define MIDPOINTSPLITTER_HPP_

#include "Primitive.hpp"

#include <algorithm>

namespace Tungsten {

namespace Bvh {

class MidpointSplitter
{
    void sort(uint32 start, uint32 end, int dim, PrimVector &prims)
    {
        std::sort(prims.begin() + start, prims.begin() + end + 1, [&](const Primitive &a, const Primitive &b) {
            if (a.centroid()[dim] == b.centroid()[dim])
                return a.id() < b.id();
            else
                return a.centroid()[dim] < b.centroid()[dim];
        });
    }

public:
    void twoWaySahSplit(uint32 start, uint32 end, PrimVector &prims, const Box3f &/*geomBox*/,
            const Box3f &centroidBox, SplitInfo &split)
    {
        split.dim = centroidBox.diagonal().maxDim();
        split.idx = (end - start + 1)/2 + start;
        sort(start, end, split.dim, prims);
        split.lBox = prims[start].box();
        split.rBox = prims[end].box();
        split.lCentroidBox = prims[start].centroid();
        split.rCentroidBox = prims[end].centroid();
        for (uint32 i = start + 1; i < end; ++i) {
            if (i < split.idx) {
                split.lBox.grow(prims[i].box());
                split.lCentroidBox.grow(prims[i].centroid());
            } else {
                split.rBox.grow(prims[i].box());
                split.rCentroidBox.grow(prims[i].centroid());
            }
        }
    }
};


}

}



#endif /* MIDPOINTSPLITTER_HPP_ */
