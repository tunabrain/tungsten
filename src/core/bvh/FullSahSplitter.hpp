#ifndef FULLSAHSPLITTER_HPP_
#define FULLSAHSPLITTER_HPP_

#include "Primitive.hpp"
#include "Splitter.hpp"

#include <algorithm>

namespace Tungsten {

namespace Bvh {

class FullSahSplitter
{
    void computeAreas(uint32 start, uint32 end, PrimVector &prims)
    {
        Box3fp rBox;
        for (uint32 i = end; i > start; --i) {
            rBox.grow(prims[i].box());
            prims[i].setArea(rBox.area());
        }
        rBox.grow(prims[start].box());
        prims[start].setArea(rBox.area());
    }

    void sort(uint32 start, uint32 end, int dim, PrimVector &prims)
    {
        std::sort(prims.begin() + start, prims.begin() + end + 1, [&](const Primitive &a, const Primitive &b) {
            if (a.centroid()[dim] == b.centroid()[dim])
                return a.id() < b.id();
            else
                return a.centroid()[dim] < b.centroid()[dim];
        });
    }

    void findSahSplit(uint32 start, uint32 end, int dim, PrimVector &prims, SplitInfo &split)
    {
        sort(start, end, dim, prims);
        computeAreas(start, end, prims);

        Box3fp lBox(prims[start].box());
        for (uint32 i = start + 1; i <= end; ++i) {
            float cost = Splitter::IntersectionCost*(lBox.area()*(i - start) + prims[i].area()*(end - i + 1));

            if (cost < split.cost) {
                split.dim = dim;
                split.idx = i;
                split.lBox = lBox;
                split.cost = cost;
            }

            lBox.grow(prims[i].box());
        }
        if (split.dim == dim) {
            Box3fp rBox;
            for (uint32 i = split.idx; i <= end; ++i)
                rBox.grow(prims[i].box());
            split.rBox = rBox;
        }
    }

public:
    void twoWaySahSplit(uint32 start, uint32 end, PrimVector &prims, const Box3f &geomBox,
            const Box3f &/*centroidBox*/, SplitInfo &split)
    {
        split.dim = -1;
        split.cost = geomBox.area()*((end - start + 1)*Splitter::IntersectionCost - Splitter::TraversalCost);

        findSahSplit(start, end, 0, prims, split);
        findSahSplit(start, end, 1, prims, split);
        findSahSplit(start, end, 2, prims, split);

        if (split.dim == -1) { /* SAH split failed, resort to midpoint split along largest extent */
            split.dim = geomBox.diagonal().maxDim();
            split.idx = (end - start + 1)/2 + start;
            sort(start, end, split.dim, prims);
            for (uint32 i = start; i <= end; ++i)
                (i < split.idx ? split.lBox : split.rBox).grow(prims[i].box());
        } else if (split.dim != 2)
            sort(start, end, split.dim, prims);
    }
};

}

}



#endif /* FULLSAHSPLITTER_HPP_ */
