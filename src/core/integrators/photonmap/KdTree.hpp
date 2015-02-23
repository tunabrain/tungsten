#ifndef KDTREE_HPP_
#define KDTREE_HPP_

#include "Photon.hpp"

#include "math/Box.hpp"
#include "math/Vec.hpp"

#include "Debug.hpp"

#include <algorithm>
#include <vector>

namespace Tungsten {

class KdTree
{
    std::vector<Photon> _nodes;
    uint32 _treeEnd;

    void recursiveTreeBuild(uint32 dst, uint32 &tail, uint32 start, uint32 end)
    {
        if (end - start == 1) {
            _nodes[dst] = _nodes[start];
            return;
        }

        if (dst >= start) {
            FAIL("Tree building error! %i >= %i", dst, start);
        }

        Box3f bounds;
        for (uint32 i = start; i < end; ++i)
            bounds.grow(_nodes[i].pos);
        uint32 splitDim = bounds.diagonal().maxDim();

        std::sort(_nodes.begin() + start, _nodes.begin() + end, [&](const Photon &a, const Photon &b) {
            return a.pos[splitDim] < b.pos[splitDim];
        });

        uint32 splitIdx = start + (end - start + 1)/2;
        _nodes[dst] = _nodes[splitIdx];
        _nodes[splitIdx] = _nodes[start++];

        uint32 childIdx = tail;
        uint32 childCount = (end - start > 2) ? 2 : 1;
        tail += childCount;

        recursiveTreeBuild(childIdx, tail, start, splitIdx + 1);
        if (childCount > 1)
            recursiveTreeBuild(childIdx + 1, tail, splitIdx + 1, end);

        _nodes[dst].setSplitInfo(childIdx, splitDim, childCount);
    }

public:
    KdTree(std::vector<PhotonType> elements, uint32 rangeStart, uint32 rangeEnd)
    : _nodes(std::move(elements))
    {
        if (rangeStart < rangeEnd) {
            uint32 tail = 1;
            recursiveTreeBuild(0, tail, rangeStart, rangeEnd);

            _treeEnd = tail;
        } else {
            _treeEnd = 0;
        }
    }

    const Photon *nearestNeighbour(Vec3f pos, float maxDist = 1e30f) const
    {
        if (_treeEnd == 0)
            return nullptr;

        const Photon *nearestPhoton = nullptr;
        float maxDistSq = maxDist*maxDist;

        const Photon *stack[28];
        const Photon **stackPtr = stack;

        const Photon *current = &_nodes[0];
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

    int nearestNeighbours(Vec3f pos, const Photon **result, float *distSq, const int k, const float maxDist = 1e30f) const
    {
        if (_treeEnd == 0)
            return 0;

        int photonCount = 0;
        float maxDistSq = maxDist*maxDist;

        const Photon *stack[28];
        const Photon **stackPtr = stack;

        const Photon *current = &_nodes[0];
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
                            const Photon *reloc = result[i];
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

    static int computePadding(int photonCount)
    {
        int numLevels = 0;
        while ((1 << numLevels) < photonCount)
            numLevels++;
        return (numLevels + 1)*2;
    }
};

}



#endif /* KDTREE_HPP_ */
