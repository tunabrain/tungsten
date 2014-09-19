#ifndef BVHBUILDER_HPP_
#define BVHBUILDER_HPP_

#include "math/MathUtil.hpp"
#include "math/Box.hpp"
#include "math/Vec.hpp"

#include "IntTypes.hpp"
#include "Debug.hpp"
#include "Timer.hpp"

#include <algorithm>
#include <future>

namespace Tungsten {

namespace Bvh {

template<uint32 BranchFactor>
class NaiveBvhNode
{
    std::array<std::unique_ptr<NaiveBvhNode>, BranchFactor> _children;
    Box3f _box;
    uint32 _id;
public:
    NaiveBvhNode() = default;

    NaiveBvhNode(const Box3f &box, uint32 id)
    : _box(box), _id(id)
    {
    }

    const NaiveBvhNode *child(int id) const
    {
        return _children[id].get();
    }

    uint32 id() const
    {
        return _id;
    }

    const Box3f &bbox() const
    {
        return _box;
    }

    bool isLeaf() const
    {
        return !_children[0];
    }

    void setChild(int id, NaiveBvhNode *child)
    {
        _children[id].reset(child);
    }

    NaiveBvhNode *child(int id)
    {
        return _children[id].get();
    }

    void setId(uint32 id)
    {
        _id = id;
    }

    Box3f &bbox()
    {
        return _box;
    }

    std::size_t dataSize()
    {
        std::size_t s = sizeof(NaiveBvhNode);
        if (!isLeaf())
            for (uint32 i = 0; i < BranchFactor; i++)
                s += _children[i]->dataSize();
        return s;
    }
};

class Primitive
{
    Box3f _box;
    Vec3f _centroid;
    uint32 _id;

    float _area;
public:
    Primitive(const Box3f &box, const Vec3f &centroid, uint32 id)
    : _box(box), _centroid(centroid), _id(id), _area(box.area())
    {
    }

    Primitive(const Vec3f &p0, const Vec3f &p1, const Vec3f &p2, uint32 id)
    : _centroid((p0 + p1 + p2)/3.0f),
      _id(id)
    {
        _box.grow(p0);
        _box.grow(p1);
        _box.grow(p2);
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

    const Box3f &box() const
    {
        return _box;
    }

    const Vec3f &centroid() const
    {
        return _centroid;
    }

    uint32 id() const
    {
        return _id;
    }
};

struct SplitInfo
{
    int dim;
    uint32 idx;
    Box3f lBox, rBox;
    Box3f lCentroidBox, rCentroidBox;
    float cost;
};

static CONSTEXPR int IntersectionCost = 1.0f;
static CONSTEXPR int TraversalCost = 1.0f;

class BinnedSahSplitter
{
    static CONSTEXPR int BinCount = 32;

    int _counts[3][BinCount];
    Box3f _geomBounds[3][BinCount];
    Box3f _centroidBounds[3][BinCount];
    Vec3f _centroidMin, _centroidSpan;

    int primitiveBin(const Primitive &prim, int dim) const
    {
        return clamp(int(BinCount*((prim.centroid()[dim] - _centroidMin[dim])/_centroidSpan[dim])), 0, BinCount - 1);
    }

    void binPrimitives(uint32 start, uint32 end, int dim, std::vector<Primitive> &prims)
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
        Box3f rBox;
        Box3f rBoxes[BinCount];
        for (int i = BinCount - 1; i > 0; --i) {
            rCount += _counts[dim][i];
            rBox.grow(_geomBounds[dim][i]);

            rCounts[i] = rCount;
            rBoxes[i] = rBox;
        }

        int lCount = _counts[dim][0];
        Box3f lBox = _geomBounds[dim][0];
        for (int i = 1; i < BinCount; ++i) {
            float cost = IntersectionCost*(lBox.area()*lCount + rBoxes[i].area()*rCounts[i]);

            if (cost < split.cost) {
                split.dim = dim;
                split.idx = i;
                split.cost = cost;
            }

            lCount += _counts[dim][i];
            lBox.grow(_geomBounds[dim][i]);
        }
    }

    uint32 sortByBin(uint32 start, uint32 end, std::vector<Primitive> &prims, int dim, int bin)
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

    void partialBin(uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &centroidBox)
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

    void twoWaySahSplit(uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &box, SplitInfo &split)
    {
        split.dim = -1;
        split.cost = box.area()*((end - start + 1)*IntersectionCost - TraversalCost);

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

    void fullSplit(uint32 start, uint32 end, std::vector<Primitive> &prims,
            const Box3f &geomBox, const Box3f &centroidBox, SplitInfo &split)
    {
        partialBin(start, end, prims, centroidBox);
        twoWaySahSplit(start, end, prims, geomBox, split);
    }
};

class MidpointSplitter
{
    void sort(uint32 start, uint32 end, int dim, std::vector<Primitive> &prims)
    {
        std::sort(prims.begin() + start, prims.begin() + end + 1, [&](const Primitive &a, const Primitive &b) {
            if (a.centroid()[dim] == b.centroid()[dim])
                return a.id() < b.id();
            else
                return a.centroid()[dim] < b.centroid()[dim];
        });
    }

public:
    void twoWaySahSplit(uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &/*geomBox*/, const Box3f &centroidBox, SplitInfo &split)
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

class FullSahSplitter
{
    void computeAreas(uint32 start, uint32 end, std::vector<Primitive> &prims)
    {
        Box3f rBox;
        for (uint32 i = end; i > start; --i) {
            rBox.grow(prims[i].box());
            prims[i].setArea(rBox.area());
        }
        rBox.grow(prims[start].box());
        prims[start].setArea(rBox.area());
    }

    void sort(uint32 start, uint32 end, int dim, std::vector<Primitive> &prims)
    {
        std::sort(prims.begin() + start, prims.begin() + end + 1, [&](const Primitive &a, const Primitive &b) {
            if (a.centroid()[dim] == b.centroid()[dim])
                return a.id() < b.id();
            else
                return a.centroid()[dim] < b.centroid()[dim];
        });
    }

    void findSahSplit(uint32 start, uint32 end, int dim, std::vector<Primitive> &prims, SplitInfo &split)
    {
        sort(start, end, dim, prims);
        computeAreas(start, end, prims);

        Box3f lBox(prims[start].box());
        for (uint32 i = start + 1; i <= end; ++i) {
            float cost = IntersectionCost*(lBox.area()*(i - start) + prims[i].area()*(end - i + 1));

            if (cost < split.cost) {
                split.dim = dim;
                split.idx = i;
                split.lBox = lBox;
                split.cost = cost;
            }

            lBox.grow(prims[i].box());
        }
        if (split.dim == dim) {
            Box3f rBox;
            for (uint32 i = split.idx; i <= end; ++i)
                rBox.grow(prims[i].box());
            split.rBox = rBox;
        }
    }

public:
    void twoWaySahSplit(uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &geomBox, const Box3f &/*centroidBox*/, SplitInfo &split)
    {
        split.dim = -1;
        split.cost = geomBox.area()*((end - start + 1)*IntersectionCost - TraversalCost);

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

template<uint32 BranchFactor>
class BvhBuilder
{
    typedef NaiveBvhNode<BranchFactor> Node;

    const int ParallelThreshold = 1000000;
    const float IntersectionCost = 1.0f;
    const float TraversalCost = 1.0f;

    std::unique_ptr<Node> _root;
    uint32 _depth;
    uint32 _numNodes;

public:
    struct BuildResult
    {
        uint32 nodeCount;
        uint32 depth;
    };

    BvhBuilder()
    : _root(new Node()),
      _depth(0),
      _numNodes(0)
    {
    }

    BuildResult recursiveBuild(Node &dst, uint32 start, uint32 end,
            std::vector<Primitive> &prims, const Box3f &geomBox, const Box3f &centroidBox)
    {
        BuildResult result{1, 1};

        dst.bbox() = geomBox;
        uint32 numPrims = end - start + 1;

        if (numPrims == 1) {
            dst.setId(prims[start].id());
        } else if (numPrims <= BranchFactor) {
            result.nodeCount += numPrims;
            for (uint32 i = start; i <= end; ++i)
                dst.setChild(i - start, new Node(prims[i].box(), prims[i].id()));
        } else {
            Vec<uint32, BranchFactor> starts(0u), ends(0u);
            std::array<Box3f, BranchFactor> geomBoxes, centroidBoxes;
            starts[0] = start;
            ends[0] = end;
            geomBoxes[0] = geomBox;
            centroidBoxes[0] = centroidBox;
            unsigned child;
            for (child = 1; child < BranchFactor; ++child) {
                uint32 interval = (ends - starts).maxDim();
                uint32 numPrims = ends[interval] - starts[interval] + 1;
                if (numPrims <= BranchFactor)
                    break;
                SplitInfo split;
                if (numPrims <= 64)
                    FullSahSplitter().twoWaySahSplit(starts[interval], ends[interval], prims, geomBoxes[interval], centroidBoxes[interval], split);
                else
                    BinnedSahSplitter().fullSplit(starts[interval], ends[interval], prims, geomBoxes[interval], centroidBoxes[interval], split);
                starts[child] = split.idx;
                ends[child] = ends[interval];
                geomBoxes[child] = split.rBox;
                centroidBoxes[child] = split.rCentroidBox;
                ends[interval] = split.idx - 1;
                geomBoxes[interval] = split.lBox;
                centroidBoxes[interval] = split.lCentroidBox;
            }

            for (unsigned i = 0; i < child; ++i) {
                dst.setChild(i, new Node());
                BuildResult recursiveResult = recursiveBuild(*dst.child(i), starts[i], ends[i], prims, geomBoxes[i], centroidBoxes[i]);
                result.nodeCount += recursiveResult.nodeCount;
                result.depth = max(result.depth, recursiveResult.depth + 1);
            }

            // TODO: Thoroughly broken parallel build code. Need to revisit this
//            std::array<std::future<BuildResult>, BranchFactor> futures;
//
//            int asyncIndex = child - 1, lazyIndex = 0;
//            for (unsigned i = 0; i < child; ++i) {
//                dst.setChild(i, new Node());
//                int index;
//                std::launch flags;
//                if (!(i == child - 1 && lazyIndex == 0) && ends[i] - starts[i] > ParallelThreshold) {
//                  index = asyncIndex--;
//                  flags = std::launch::async;
//                } else {
//                  index = lazyIndex++;
//                  flags = std::launch::deferred;
//                }
//              futures[index] = std::async(flags, &BvhBuilder<BranchFactor>::recursiveBuild,
//                      this, std::ref(*dst.child(i)), starts[i], ends[i], std::ref(prims), std::cref(boxes[i]));
//            }
//            for (unsigned i = 0; i < child; ++i) {
//              BuildResult recursiveResult = futures[i].get();
//              result.nodeCount += recursiveResult.nodeCount;
//              result.depth = max(result.depth, recursiveResult.depth + 1);
//            }
        }

        return result;
    }

    void build(std::vector<Primitive> prims)
    {
        if (prims.empty())
            return;
//#ifndef NDEBUG
        Timer timer;
//#endif
        Box3f geomBounds, centroidBounds;
        for (const Primitive &p : prims) {
            geomBounds.grow(p.box());
            centroidBounds.grow(p.centroid());
        }

        BuildResult result = recursiveBuild(*_root, 0, prims.size() - 1, prims, geomBounds, centroidBounds);
        _numNodes = result.nodeCount;
        _depth = result.depth;

//#ifndef NDEBUG
        timer.bench("Recursive build finished");

        integrityCheck(*_root, 0);
//#endif
    }

    void integrityCheck(const Node &node, int depth) const
    {
        if (node.isLeaf())
            return;
        for (unsigned i = 0; i < BranchFactor && node.child(i); ++i) {
            integrityCheck(*node.child(i), depth + 1);
            ASSERT(node.bbox().contains(node.child(i)->bbox()),
                    "Child box not contained! %s c/ %s at %d",
                    node.child(i)->bbox(), node.bbox(), depth);
        }
    }

    std::unique_ptr<Node> &root()
    {
        return _root;
    }

    uint32 depth() const
    {
        return _depth;
    }

    uint32 numNodes() const
    {
        return _numNodes;
    }
};

}

}

#endif /* BVHBUILDER_HPP_ */
