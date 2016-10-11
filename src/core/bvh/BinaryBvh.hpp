#ifndef BINARYBVH_HPP_
#define BINARYBVH_HPP_

#include "BvhBuilder.hpp"

#include "math/Box.hpp"
#include "math/Vec.hpp"
#include "sse/SimdUtils.hpp"

#include "IntTypes.hpp"

namespace Tungsten {

#include "AlignedAllocator.hpp"

typedef Vec<float4, 3> Vec3pf;

namespace Bvh {

class BinaryBvh
{
    template<typename T> using aligned_vector = std::vector<T, AlignedAllocator<T, 64>>;


    Vec3pf transpose(const Vec3f &p) const
    {
        return Vec3pf(float4(p.x()), float4(p.y()), float4(p.z()));
    }

    class TinyBvhNode
    {
        union PointerOrIndex {
            TinyBvhNode *node;
            uint64 index;
        };

        Vec3pf _box;
        PointerOrIndex _lChild;
        PointerOrIndex _rChild;

    public:
        const Vec3pf &bbox() const
        {
            return _box;
        }

        void setJointBbox(const Box3f &lbox, const Box3f &rbox)
        {
            _box = Vec3pf(
                float4(lbox.min().x(), rbox.min().x(), lbox.max().x(), rbox.max().x()),
                float4(lbox.min().y(), rbox.min().y(), lbox.max().y(), rbox.max().y()),
                float4(lbox.min().z(), rbox.min().z(), lbox.max().z(), rbox.max().z())
            );
        }

        uint32 primIndex() const
        {
            return _lChild.index & 0xFFFFFFFFU;
        }

        uint32 childCount() const
        {
            return _lChild.index >> 32;
        }

        void setPrimIndex(uint32 count, uint32 idx)
        {
            _lChild.index = (uint64(count) << 32) | uint64(idx);
        }

        TinyBvhNode *lChild()
        {
            return _lChild.node;
        }

        const TinyBvhNode *lChild() const
        {
            return _lChild.node;
        }

        void setLchild(TinyBvhNode *child)
        {
            _lChild.node = child;
        }

        TinyBvhNode *rChild()
        {
            return _rChild.node;
        }

        const TinyBvhNode *rChild() const
        {
            return _rChild.node;
        }

        void setRchild(TinyBvhNode *child)
        {
            _rChild.node = child;
        }

        bool isLeaf() const
        {
            return _rChild.node == nullptr;
        }

        bool isNode() const
        {
            return _rChild.node != nullptr;
        }
    };

    int _depth;
    aligned_vector<TinyBvhNode> _nodes;
    std::vector<uint32> _primIndices;
    Box3f _bounds;

    uint32 recursiveBuild(const NaiveBvhNode *node, uint32 head, uint32 &tail, uint32 &primIndex,
            std::vector<uint32> &primIndices, uint32 maxPrimsPerLeaf)
    {
        if (node->isLeaf()) {
            _nodes[head].setJointBbox(node->bbox(), node->bbox());
            _nodes[head].setPrimIndex(1, primIndex);
            _nodes[head].setRchild(nullptr);
            primIndices[primIndex++] = node->id();

            return 1;
        } else {
            int childIdx = tail;
            _nodes[head].setJointBbox(node->child(0)->bbox(), node->child(1)->bbox());
            _nodes[head].setLchild(&_nodes[childIdx + 0]);
            _nodes[head].setRchild(&_nodes[childIdx + 1]);
            tail += 2;
            uint32 lPrims = recursiveBuild(node->child(0), childIdx + 0, tail, primIndex, primIndices, maxPrimsPerLeaf);
            uint32 rPrims = recursiveBuild(node->child(1), childIdx + 1, tail, primIndex, primIndices, maxPrimsPerLeaf);

            if (lPrims + rPrims <= maxPrimsPerLeaf) {
                _nodes[head].setPrimIndex(lPrims + rPrims, _nodes[childIdx].primIndex());
                _nodes[head].setRchild(nullptr);
                tail = childIdx;
            }

            return lPrims + rPrims;
        }
    }

    bool bboxIntersection(const Box3f &box, const Vec3f &o, const Vec3f &d, float &tMin, float &tMax) const
    {
        Vec3f invD = 1.0f/d;
        Vec3f relMin((box.min() - o));
        Vec3f relMax((box.max() - o));

        float ttMin = tMin, ttMax = tMax;
        for (int i = 0; i < 3; ++i) {
            if (invD[i] >= 0.0f) {
                ttMin = max(ttMin, relMin[i]*invD[i]);
                ttMax = min(ttMax, relMax[i]*invD[i]);
            } else {
                ttMax = min(ttMax, relMin[i]*invD[i]);
                ttMin = max(ttMin, relMax[i]*invD[i]);
            }
        }

        if (ttMin <= ttMax) {
            tMin = ttMin;
            tMax = ttMax;
            return true;
        }
        return false;
    }

public:
    BinaryBvh(PrimVector prims, int maxPrimsPerLeaf)
    {
        size_t count = prims.size();

        if (prims.empty()) {
            _depth = 0;
            _nodes.push_back(TinyBvhNode());
            _nodes.back().setJointBbox(Box3f(), Box3f());
            _nodes.back().setRchild(&_nodes.back());
        } else {
            BvhBuilder builder(2);
            builder.build(std::move(prims));

            _primIndices.resize(count);
            _nodes.resize(builder.numNodes());
            _depth = builder.depth();
            _bounds = builder.root()->bbox();

            uint32 tail = 1, primIndex = 0;
            recursiveBuild(builder.root().get(), 0, tail, primIndex, _primIndices, maxPrimsPerLeaf);
            builder.root().reset();
            _nodes.resize(tail);
        }
    }

    template<typename LAMBDA>
    void trace(Ray &ray, LAMBDA intersector) const
    {
        struct StackNode
        {
            const TinyBvhNode *node;
            float tMin;

            void set(const TinyBvhNode *n, float t)
            {
                node = n;
                tMin = t;
            }
        };
        StackNode *stack = reinterpret_cast<StackNode *>(alloca((_depth + 1)*sizeof(StackNode)));
        StackNode *stackPtr = stack;

        const TinyBvhNode *node = &_nodes[0];
        float tMin = ray.nearT(), tMax = ray.farT();
        if (!bboxIntersection(_bounds, ray.pos(), ray.dir(), tMin, tMax))
            return;

        const float4 signMask(_mm_castsi128_ps(_mm_set_epi32(0x80000000, 0x80000000, 0x00000000, 0x00000000)));
        const __m128i keepNearFar = _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8,  7,  6,  5,  4,  3,  2, 1, 0);
        const __m128i swapNearFar = _mm_set_epi8( 7,  6,  5,  4,  3,  2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
        const __m128i xMask = ray.dir().x() >= 0.0f ? keepNearFar : swapNearFar;
        const __m128i yMask = ray.dir().y() >= 0.0f ? keepNearFar : swapNearFar;
        const __m128i zMask = ray.dir().z() >= 0.0f ? keepNearFar : swapNearFar;

        const Vec3pf rayO(transpose(ray.pos()));
        const Vec3pf invDir = float4(1.0f)/transpose(ray.dir());
        const Vec3pf invNegDir(invDir.x() ^ signMask, invDir.y() ^ signMask, invDir.z() ^ signMask);

        uint32 start, count;
        float4 nearFar(ray.nearT(), ray.nearT(), -ray.farT(), -ray.farT());
        while (true) {
            while (node->isNode()) {
                const Vec3pf tNearFar = Vec3pf(
                    float4(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128((node->bbox().x() - rayO.x()).raw()), xMask))),
                    float4(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128((node->bbox().y() - rayO.y()).raw()), yMask))),
                    float4(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128((node->bbox().z() - rayO.z()).raw()), zMask)))
                )*invNegDir;
                float4 minMax = max(tNearFar.x(), tNearFar.y(), tNearFar.z(), nearFar);
                minMax ^= signMask;
                float4 maxMin(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(minMax.raw()), swapNearFar)));
                bool4 hit = minMax <= maxMin;

                bool intersectsL = hit[0];
                bool intersectsR = hit[1];

                if (intersectsL && intersectsR) {
                    if (minMax[0] < minMax[1]) {
                        stackPtr++->set(node->rChild(), minMax[1]);
                        node = node->lChild();
                        tMin = minMax[0];
                    } else {
                        stackPtr++->set(node->lChild(), minMax[0]);
                        node = node->rChild();
                        tMin = minMax[1];
                    }
                } else if (intersectsL) {
                    node = node->lChild();
                    tMin = minMax[0];
                } else if (intersectsR) {
                    node = node->rChild();
                    tMin = minMax[1];
                } else {
                    goto pop;
                }
            }

            start = node->primIndex();
            count = node->childCount();
            for (uint32 i = start; i < start + count; ++i)
                intersector(ray, _primIndices[i], tMin, node->bbox());
            tMax = min(tMax, ray.farT());
            nearFar[2] = nearFar[3] = -tMax;

pop:
            if (stackPtr-- == stack)
                return;
            node = stackPtr->node;
            tMin = stackPtr->tMin;
            if (tMax < tMin)
                goto pop;
        }
    }
};

}

}

#endif /* BINARYBVH_HPP_ */
