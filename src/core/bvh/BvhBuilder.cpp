#include "BvhBuilder.hpp"

#include "BinnedSahSplitter.hpp"
#include "MidpointSplitter.hpp"
#include "FullSahSplitter.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "math/MathUtil.hpp"
#include "math/Box.hpp"
#include "math/Vec.hpp"

#include "IntTypes.hpp"
#include "Debug.hpp"

#include <algorithm>

namespace Tungsten {

namespace Bvh {

struct BuildResult
{
    uint32 nodeCount;
    uint32 depth;
};

static void twoWaySahSplit(uint32 start, uint32 end, PrimVector &prims, const Box3f &geomBox,
        const Box3f &centroidBox, SplitInfo &split)
{
    uint32 numPrims = end - start + 1;

    if (numPrims <= 64) {
        // O(n log n) exact SAH split for small workloads
        FullSahSplitter().twoWaySahSplit(start, end, prims, geomBox, centroidBox, split);
    } else if (numPrims <= 1024*1024) {
        // O(n) approximate binned SAH split for medium workloads
        BinnedSahSplitter().fullSplit(start, end, prims, geomBox, centroidBox, split);
    } else {
        // Parallel O(n) approximate binned SAH split with
        // serial reduce for large workloads

        CONSTEXPR uint32 NumTasks = 8;
        BinnedSahSplitter splitters[NumTasks];

        std::shared_ptr<TaskGroup> group = ThreadUtils::pool->enqueue([&](uint32 i, uint32 numTasks, uint32) {
            uint32 span = numPrims/numTasks;
            uint32 primStart = start + span*i;
            uint32 primEnd = min(primStart + span, end);
            splitters[i].partialBin(primStart, primEnd, prims, centroidBox);
        }, NumTasks);
        // Do some work while we wait
        ThreadUtils::pool->yield(*group);

        for (uint32 i = 1; i < NumTasks; ++i)
            splitters[0].merge(splitters[i]);

        splitters[0].twoWaySahSplit(start, end, prims, geomBox, split);
    }
}

static uint32 sahSplit(uint32 starts[], uint32 ends[], Box3f geomBoxes[],
        Box3f centroidBoxes[], PrimVector &prims, uint32 branchFactor)
{
    uint32 childCount;

    for (childCount = 1; childCount < branchFactor; ++childCount) {
        // Find child with most primitives
        uint32 interval = 0;
        for (uint32 i = 1; i < childCount; ++i)
            if (ends[interval] - starts[interval] < ends[i] - starts[i])
                interval = i;

        // Largest child fits into a leaf node? -> we're done
        uint32 numPrims = ends[interval] - starts[interval] + 1;
        if (numPrims <= branchFactor)
            break;

        // If not, split the largest child
        SplitInfo split;
        twoWaySahSplit(starts[interval], ends[interval], prims, geomBoxes[interval],
                centroidBoxes[interval], split);

        // Create two new children
        starts       [childCount] = split.idx;
        ends         [childCount] = ends[interval];
        geomBoxes    [childCount] = narrow(split.rBox);
        centroidBoxes[childCount] = narrow(split.rCentroidBox);

        ends         [interval] = split.idx - 1;
        geomBoxes    [interval] = narrow(split.lBox);
        centroidBoxes[interval] = narrow(split.lCentroidBox);
    }

    return childCount;
}

static void recursiveBuild(BuildResult &result, NaiveBvhNode &dst, uint32 start, uint32 end,
        PrimVector &prims, const Box3f &geomBox, const Box3f &centroidBox, uint32 branchFactor)
{
    result = BuildResult{1, 1};

    dst.bbox() = geomBox;
    uint32 numPrims = end - start + 1;

    if (numPrims == 1) {
        // Single primitive, just create a leaf node
        dst.setId(prims[start].id());
    } else if (numPrims <= branchFactor) {
        // Few primitives, create internal node with numPrims children
        result.nodeCount += numPrims;
        for (uint32 i = start; i <= end; ++i)
            dst.setChild(i - start, new NaiveBvhNode(narrow(prims[i].box()), prims[i].id()));
    } else {
        // Many primitives: Setup SAH split
        uint32 starts[4], ends[4];
        Box3f geomBoxes[4], centroidBoxes[4];
        starts       [0] = start;
        ends         [0] = end;
        geomBoxes    [0] = geomBox;
        centroidBoxes[0] = centroidBox;

        // Perform the split (potentially in parallel)
        uint32 childCount = sahSplit(starts, ends, geomBoxes, centroidBoxes, prims, branchFactor);

        // TODO: Use pool allocator?
        for (unsigned i = 0; i < childCount; ++i)
            dst.setChild(i, new NaiveBvhNode());

        if (numPrims <= 32*1024) {
            // Perform single threaded recursive build for small workloads
            for (unsigned i = 0; i < childCount; ++i) {
                BuildResult recursiveResult;
                recursiveBuild(recursiveResult, *dst.child(i), starts[i], ends[i],
                        prims, geomBoxes[i], centroidBoxes[i], branchFactor);
                result.nodeCount += recursiveResult.nodeCount;
                result.depth = max(result.depth, recursiveResult.depth + 1);
            }
        } else {
            // Enqueue parallel build for large workloads
            BuildResult results[4];
            std::shared_ptr<TaskGroup> group = ThreadUtils::pool->enqueue([&](uint32 i, uint32, uint32) {
                recursiveBuild(results[i], *dst.child(i), starts[i], ends[i],
                        prims, geomBoxes[i], centroidBoxes[i], branchFactor);
            }, childCount);
            // Do some work while we wait
            ThreadUtils::pool->yield(*group);

            // Serial reduce
            for (unsigned i = 0; i < childCount; ++i) {
                result.nodeCount += results[i].nodeCount;
                result.depth = max(result.depth, results[i].depth + 1);
            }
        }
    }
}

BvhBuilder::BvhBuilder(uint32 branchFactor)
: _root(new NaiveBvhNode()),
  _depth(0),
  _numNodes(0),
  _branchFactor(branchFactor)
{
}

void BvhBuilder::build(PrimVector prims)
{
    if (prims.empty())
        return;
    Box3fp geomBounds, centroidBounds;
    for (const Primitive &p : prims) {
        geomBounds.grow(p.box());
        centroidBounds.grow(p.centroid());
    }

    BuildResult result;
    recursiveBuild(result, *_root, 0, uint32(prims.size() - 1), prims, narrow(geomBounds), narrow(centroidBounds), _branchFactor);
    _numNodes = result.nodeCount;
    _depth = result.depth;

#ifndef NDEBUG
    integrityCheck(*_root, 0);
#endif
}

void BvhBuilder::integrityCheck(const NaiveBvhNode &node, int depth) const
{
    if (node.isLeaf())
        return;
    for (unsigned i = 0; i < _branchFactor && node.child(i); ++i) {
        integrityCheck(*node.child(i), depth + 1);
        ASSERT(node.bbox().contains(node.child(i)->bbox()),
                "Child box not contained! %s c/ %s at %d",
                node.child(i)->bbox(), node.bbox(), depth);
    }
}

}

}
