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

template<uint32 BranchFactor>
class BvhBuilder
{
    static CONSTEXPR int ParallelThreshold = 1000000;

    struct SplitInfo
    {
        int dim;
        uint32 idx;
        Box3f lBox, rBox;
        float cost;
    };

    typedef NaiveBvhNode<BranchFactor> Node;

    const float IntersectionCost = 1.0f;
    const float TraversalCost = 1.0f;

protected:
    std::unique_ptr<Node> _root;
    uint32 _depth;
    uint32 _numNodes;

public:
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

    void twoWaySahSplit(uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &box, SplitInfo &split)
    {
        split.dim = -1;
        split.cost = box.area()*((end - start + 1)*IntersectionCost - TraversalCost);

        findSahSplit(start, end, 0, prims, split);
        findSahSplit(start, end, 1, prims, split);
        findSahSplit(start, end, 2, prims, split);

        if (split.dim == -1) { /* SAH split failed, resort to midpoint split along largest extent */
            split.dim = box.diagonal().maxDim();
            split.idx = (end - start + 1)/2 + start;
            sort(start, end, split.dim, prims);
            for (uint32 i = start; i <= end; ++i)
                (i < split.idx ? split.lBox : split.rBox).grow(prims[i].box());
        } else if (split.dim != 2)
            sort(start, end, split.dim, prims);
    }

    BuildResult recursiveBuild(Node &dst, uint32 start, uint32 end, std::vector<Primitive> &prims, const Box3f &box)
    {
        BuildResult result{1, 1};

        dst.bbox() = box;
        uint32 numPrims = end - start + 1;

        if (numPrims == 1) {
            dst.setId(prims[start].id());
        } else if (numPrims <= BranchFactor) {
            result.nodeCount += numPrims;
            for (uint32 i = start; i <= end; ++i)
                dst.setChild(i - start, new Node(prims[i].box(), prims[i].id()));
        } else {
            Vec<uint32, BranchFactor> starts(0u), ends(0u);
            std::array<Box3f, BranchFactor> boxes;
            starts[0] = start;
            ends[0] = end;
            boxes[0] = box;
            unsigned child;
            for (child = 1; child < BranchFactor; ++child) {
                uint32 interval = (ends - starts).maxDim();
                if (ends[interval] - starts[interval] + 1 <= BranchFactor)
                    break;
                SplitInfo split;
                twoWaySahSplit(starts[interval], ends[interval], prims, boxes[interval], split);
                starts[child] = split.idx;
                ends[child] = ends[interval];
                boxes[child] = split.rBox;
                ends[interval] = split.idx - 1;
                boxes[interval] = split.lBox;
            }

            for (unsigned i = 0; i < child; ++i) {
                dst.setChild(i, new Node());
                BuildResult recursiveResult = recursiveBuild(*dst.child(i), starts[i], ends[i], prims, boxes[i]);
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
#ifndef NDEBUG
        Timer timer;
#endif
        Box3f bounds;
        for (const Primitive &p : prims)
            bounds.grow(p.box());

        BuildResult result = recursiveBuild(*_root, 0, prims.size() - 1, prims, bounds);
        _numNodes = result.nodeCount;
        _depth = result.depth;

#ifndef NDEBUG
        timer.bench("Recursive build finished");

        integrityCheck(*_root, 0);
#endif
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

#endif /* BVHBUILDER_HPP_ */
