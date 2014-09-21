#ifndef BINNEDSAHSPLITTER_HPP_
#define BINNEDSAHSPLITTER_HPP_

#include "Primitive.hpp"
#include "Splitter.hpp"

#include <algorithm>

namespace Tungsten {

namespace Bvh {

class BinnedSahSplitter
{
    static CONSTEXPR int BinCount = 32;

    Box3fp _geomBounds[3][BinCount];
    Box3fp _centroidBounds[3][BinCount];
    Vec3f _centroidMin, _centroidSpan;
    int _counts[3][BinCount];

    int primitiveBin(const Primitive &prim, int dim) const
    {
        return clamp(int(BinCount*((prim.centroid()[dim] - _centroidMin[dim])/_centroidSpan[dim])), 0, BinCount - 1);
    }

    void binPrimitives(uint32 start, uint32 end, int dim, PrimVector &prims)
    {
        for (uint32 i = start; i <= end; ++i) {
            int idx = primitiveBin(prims[i], dim);
            _geomBounds[dim][idx].grow(prims[i].box());
            _centroidBounds[dim][idx].grow(prims[i].centroid());
            _counts[dim][idx]++;
        }
    }

    void findSahSplit(int dim, SplitInfo &split)
    {
        int rCount = 0;
        int rCounts[BinCount];
        Box3fp rBox;
        Box3fp rBoxes[BinCount];
        for (int i = BinCount - 1; i > 0; --i) {
            rCount += _counts[dim][i];
            rBox.grow(_geomBounds[dim][i]);

            rCounts[i] = rCount;
            rBoxes[i] = rBox;
        }

        int lCount = _counts[dim][0];
        Box3fp lBox = _geomBounds[dim][0];
        for (int i = 1; i < BinCount; ++i) {
            float cost = Splitter::IntersectionCost*(lBox.area()*lCount + rBoxes[i].area()*rCounts[i]);

            if (cost < split.cost) {
                split.dim = dim;
                split.idx = i;
                split.cost = cost;
            }

            lCount += _counts[dim][i];
            lBox.grow(_geomBounds[dim][i]);
        }
    }

    uint32 sortByBin(uint32 start, uint32 end, PrimVector &prims, int dim, int bin)
    {
        uint32 left = start, right = end;
        while (left < right) {
            while (left < right && primitiveBin(prims[left], dim) < bin)
                left++;
            while (right > left && primitiveBin(prims[right], dim) >= bin)
                right--;
            if (left != right)
                std::swap(prims[left], prims[right]);
        }
        // Degenerate case - return one past end
        if (left == end || right == start)
            return end + 1;
        return left;
    }

public:
    BinnedSahSplitter()
    {
        std::memset(_counts, 0, sizeof(_counts));
    }

    void partialBin(uint32 start, uint32 end, PrimVector &prims, const Box3f &centroidBox)
    {
        _centroidMin = centroidBox.min();
        _centroidSpan = centroidBox.diagonal();
        for (int i = 0; i < 3; ++i)
            if (_centroidSpan[i] > 0.0f)
                binPrimitives(start, end, i, prims);
    }

    void merge(const BinnedSahSplitter &o)
    {
        for (int i = 0; i < 3; ++i) {
            for (int t = 0; t < BinCount; ++t) {
                _geomBounds[i][t].grow(o._geomBounds[i][t]);
                _centroidBounds[i][t].grow(o._centroidBounds[i][t]);
                _counts[i][t] += o._counts[i][t];
            }
        }
    }

    void twoWaySahSplit(uint32 start, uint32 end, PrimVector &prims, const Box3f &box, SplitInfo &split)
    {
        split.dim = -1;
        split.cost = box.area()*((end - start + 1)*Splitter::IntersectionCost - Splitter::TraversalCost);

        for (int i = 0; i < 3; ++i)
            if (_centroidSpan[i] > 0.0f)
                findSahSplit(i, split);

        if (split.dim == -1) { /* SAH split failed, resort to midpoint split along largest extent */
            split.dim = box.diagonal().maxDim();
            split.idx = BinCount/2;
        }
        int bin = split.idx;
        split.idx = sortByBin(start, end, prims, split.dim, bin);

        if (split.idx > end || _centroidSpan == 0.0f) { /* Degenerate case */
            split.idx = start + (end - start + 1)/2;
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
        } else {
            split.lBox = _geomBounds[split.dim][0];
            split.rBox = _geomBounds[split.dim][BinCount - 1];
            split.lCentroidBox = _centroidBounds[split.dim][0];
            split.rCentroidBox = _centroidBounds[split.dim][BinCount - 1];

            for (int i = 1; i < BinCount - 1; ++i) {
                if (i < bin) {
                    split.lBox.grow(_geomBounds[split.dim][i]);
                    split.lCentroidBox.grow(_centroidBounds[split.dim][i]);
                } else {
                    split.rBox.grow(_geomBounds[split.dim][i]);
                    split.rCentroidBox.grow(_centroidBounds[split.dim][i]);
                }
            }
        }
    }

    void fullSplit(uint32 start, uint32 end, PrimVector &prims,
            const Box3f &geomBox, const Box3f &centroidBox, SplitInfo &split)
    {
        partialBin(start, end, prims, centroidBox);
        twoWaySahSplit(start, end, prims, geomBox, split);
    }
};

}

}



#endif /* BINNEDSAHSPLITTER_HPP_ */
