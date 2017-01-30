#ifndef KDTREE_HPP_
#define KDTREE_HPP_

#include "math/Box.hpp"
#include "math/Vec.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"
#include "thread/TaskGroup.hpp"

#include <algorithm>
#include <vector>

namespace Tungsten {

template<typename PhotonType>
class KdTree
{
    PhotonType *_nodes;
    uint32 _treeEnd;

    void recursiveTreeBuild(uint32 dst, uint32 start, uint32 end)
    {
        if (end == start) {
            // Leaf node
            _nodes[dst].setSplitInfo(0, 0, 0);
            return;
        } else if (end - start == 1) {
            // Single child only. Special case
            if (_nodes[dst].pos.x() < _nodes[start].pos.x())
                std::swap(_nodes[dst], _nodes[start]);
            _nodes[dst].setSplitInfo(start, 0, 1);
            _nodes[start].setSplitInfo(0, 0, 0);
            return;
        }

        Box3f bounds(_nodes[dst].pos);
        for (uint32 i = start; i < end; ++i)
            bounds.grow(_nodes[i].pos);
        uint32 splitDim = bounds.diagonal().maxDim();

        std::sort(_nodes + start, _nodes + end, [&](const PhotonType &a, const PhotonType &b) {
            return a.pos[splitDim] < b.pos[splitDim];
        });

        uint32 splitIdx = start + (end - start + 1)/2;
        float rightPlane = _nodes[splitIdx].pos[splitDim];
        float  headPlane = _nodes[dst].pos[splitDim];
        float  leftPlane = _nodes[splitIdx - 1].pos[splitDim];

        if (headPlane < leftPlane || headPlane > rightPlane) {
            uint32 swapIdx = headPlane > rightPlane ? splitIdx : splitIdx - 1;
            std::swap(_nodes[dst], _nodes[swapIdx]);
        }

        uint32 childIdx = start;
        if (splitIdx > childIdx + 1)
            std::swap(_nodes[childIdx + 1], _nodes[splitIdx]);

        std::shared_ptr<TaskGroup> group;
        if (splitIdx - start > 100000) {
            group = ThreadUtils::pool->enqueue([&](uint32, uint32, uint32) {
                recursiveTreeBuild(childIdx + 0, start + 2, splitIdx + 1);
            }, 1, [](){});
        } else {
            recursiveTreeBuild(childIdx + 0, start + 2, splitIdx + 1);
        }
        recursiveTreeBuild(childIdx + 1, splitIdx + 1, end);

        if (group && !group->isDone())
            ThreadUtils::pool->yield(*group);

        _nodes[dst].setSplitInfo(childIdx, splitDim, 2);
    }

    void buildVolumeHierarchy(uint32 root)
    {
        Box3f bounds(_nodes[root].pos);
        bounds.grow(std::sqrt(_nodes[root].radiusSq));

        uint32 childIdx = _nodes[root].childIdx();
        if (_nodes[root].hasLeftChild()) {
            buildVolumeHierarchy(childIdx);
            bounds.grow(_nodes[childIdx].minBounds);
            bounds.grow(_nodes[childIdx].maxBounds);
        }
        if (_nodes[root].hasRightChild()) {
            buildVolumeHierarchy(childIdx + 1);
            bounds.grow(_nodes[childIdx + 1].minBounds);
            bounds.grow(_nodes[childIdx + 1].maxBounds);
        }

        _nodes[root].minBounds = bounds.min();
        _nodes[root].maxBounds = bounds.max();
    }

public:
    KdTree(PhotonType *elements, uint32 rangeEnd)
    : _nodes(elements)
    {
        if (rangeEnd > 0)
            recursiveTreeBuild(0, 1, rangeEnd);
        _treeEnd = rangeEnd;
    }

    void buildVolumeHierarchy(bool fixedRadius, float radiusScale)
    {
        if (fixedRadius) {
            for (uint32 i = 0; i < _treeEnd; ++i)
                _nodes[i].radiusSq = radiusScale*radiusScale;
        } else {
            int m = min(30, int(_treeEnd));
            float scale = radiusScale*(std::sqrt(float(_treeEnd))*0.05f)/float(m);
            ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue([&](uint32 idx, uint32 num, uint32 /*threadId*/) {
                uint32 span = (_treeEnd + num - 1)/num;
                uint32 start = span*idx;
                uint32 end = min(start + span, _treeEnd);

                std::unique_ptr<const PhotonType *[]> photons(new const PhotonType *[m]);
                std::unique_ptr<float[]> dists(new float[m]);
                for (uint32 i = start; i < end; ++i) {
                    nearestNeighbours(_nodes[i].pos, photons.get(), dists.get(), m);
                    _nodes[i].radiusSq = dists[0]*scale;
                }
            }, ThreadUtils::pool->threadCount()));
        }

        buildVolumeHierarchy(0);
    }

    const PhotonType *nearestNeighbour(Vec3f pos, float maxDist = 1e30f) const
    {
        if (_treeEnd == 0)
            return nullptr;

        const PhotonType *nearestPhoton = nullptr;
        float maxDistSq = maxDist*maxDist;

        const PhotonType *stack[28];
        const PhotonType **stackPtr = stack;

        const PhotonType *current = &_nodes[0];
        while (true) {
            float dSq = (current->pos - pos).lengthSq();
            if (dSq < maxDistSq) {
                maxDistSq = dSq;
                nearestPhoton = current;
            }

            uint32 splitDim = current->splitDim();
            float planeDist = pos[splitDim] - current->pos[splitDim];
            bool traverseLeft  = current->hasLeftChild () && (planeDist <= 0.0f || planeDist*planeDist < maxDistSq);
            bool traverseRight = current->hasRightChild() && (planeDist >= 0.0f || planeDist*planeDist < maxDistSq);

            uint32 childIdx = current->childIdx();
            if (traverseLeft && traverseRight) {
                if (planeDist <= 0.0f) {
                    *stackPtr++ = &_nodes[childIdx + 1];
                    current = &_nodes[childIdx];
                } else {
                    *stackPtr++ = &_nodes[childIdx];
                    current = &_nodes[childIdx + 1];
                }
            } else if (traverseLeft) {
                current = &_nodes[childIdx];
            } else if (traverseRight) {
                current = &_nodes[childIdx + 1];
            } else {
                if (stackPtr == stack)
                    return nearestPhoton;
                else
                    current = *--stackPtr;
            }
        }
        return nullptr;
    }

    int nearestNeighbours(Vec3f pos, const PhotonType **result, float *distSq, const int k, const float maxDist = 1e30f) const
    {
        if (_treeEnd == 0)
            return 0;

        int photonCount = 0;
        float maxDistSq = maxDist*maxDist;

        const PhotonType *stack[28];
        const PhotonType **stackPtr = stack;

        const PhotonType *current = &_nodes[0];
        while (true) {
            float dSq = (current->pos - pos).lengthSq();
            if (dSq < maxDistSq) {
                if (photonCount < k) {
                    result[photonCount] = current;
                    distSq[photonCount] = dSq;
                    photonCount++;

                    if (photonCount == k) {
                        // Build max heap
                        const int halfK = k/2;
                        for (int i = halfK - 1; i >= 0; --i) {
                            int parent = i;
                            const PhotonType *reloc = result[i];
                            float relocDist = distSq[i];
                            while (parent < halfK) {
                                int child = parent*2 + 1;
                                if (child < k - 1 && distSq[child] < distSq[child + 1])
                                    child++;
                                if (relocDist >= distSq[child])
                                    break;
                                result[parent] = result[child];
                                distSq[parent] = distSq[child];
                                parent = child;
                            }
                            result[parent] = reloc;
                            distSq[parent] = relocDist;
                        }
                        maxDistSq = distSq[0];
                    }
                } else {
                    const int halfK = k/2;
                    int parent = 0;
                    while (parent < halfK) {
                        int child = parent*2 + 1;
                        if (child < k - 1 && distSq[child] < distSq[child + 1])
                            child++;
                        if (dSq >= distSq[child])
                            break;
                        result[parent] = result[child];
                        distSq[parent] = distSq[child];
                        parent = child;
                    }
                    result[parent] = current;
                    distSq[parent] = dSq;
                    maxDistSq = distSq[0];
                }
            }

            uint32 splitDim = current->splitDim();
            float planeDist = pos[splitDim] - current->pos[splitDim];
            bool traverseLeft  = current->hasLeftChild () && (planeDist <= 0.0f || planeDist*planeDist < maxDistSq);
            bool traverseRight = current->hasRightChild() && (planeDist >= 0.0f || planeDist*planeDist < maxDistSq);

            uint32 childIdx = current->childIdx();
            if (traverseLeft && traverseRight) {
                if (planeDist <= 0.0f) {
                    *stackPtr++ = &_nodes[childIdx + 1];
                    current = &_nodes[childIdx];
                } else {
                    *stackPtr++ = &_nodes[childIdx];
                    current = &_nodes[childIdx + 1];
                }
            } else if (traverseLeft) {
                current = &_nodes[childIdx];
            } else if (traverseRight) {
                current = &_nodes[childIdx + 1];
            } else {
                if (stackPtr == stack)
                    return photonCount;
                else
                    current = *--stackPtr;
            }
        }
        return 0;
    }

    template<typename Traverser>
    inline void beamQuery(Vec3f pos, Vec3f dir, float farT, Traverser traverser) const
    {
        if (_treeEnd == 0)
            return;

        const Vec3f invDir = 1.0f/dir;

        const PhotonType *stack[28];
        const PhotonType **stackPtr = stack;

        const PhotonType *current = &_nodes[0];
        while (true) {
            Vec3f mins = (current->minBounds - pos)*invDir;
            Vec3f maxs = (current->maxBounds - pos)*invDir;
            float minT = max(
                invDir[0] > 0.0f ? mins[0] : maxs[0],
                invDir[1] > 0.0f ? mins[1] : maxs[1],
                invDir[2] > 0.0f ? mins[2] : maxs[2]
            );
            float maxT = min(
                invDir[0] > 0.0f ? maxs[0] : mins[0],
                invDir[1] > 0.0f ? maxs[1] : mins[1],
                invDir[2] > 0.0f ? maxs[2] : mins[2]
            );

            if (minT <= maxT && minT <= farT && maxT >= 0.0f) {
                Vec3f p = current->pos - pos;
                float proj = p.dot(dir);
                if (proj >= 0.0f && proj <= farT) {
                    float distSq = p.lengthSq() - proj*proj;
                    if (distSq <= current->radiusSq)
                        traverser(*current, proj, distSq);
                }

                uint32 childIdx = current->childIdx();
                if (current->hasLeftChild() && current->hasRightChild()) {
                    *stackPtr++ = &_nodes[childIdx + 1];
                    current     = &_nodes[childIdx + 0];
                } else if (current->hasLeftChild()) {
                    current = &_nodes[childIdx];
                } else if (current->hasRightChild()) {
                    current = &_nodes[childIdx + 1];
                } else {
                    goto pop;
                }

                continue;
            }

        pop:
            if (stackPtr == stack)
                break;
            current = *--stackPtr;
        }
    }

    std::vector<PhotonType> release()
    {
        return std::move(_nodes);
    }
};

}



#endif /* KDTREE_HPP_ */
