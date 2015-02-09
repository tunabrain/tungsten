#ifndef SOLIDANGLEBVH_HPP_
#define SOLIDANGLEBVH_HPP_

#include "math/Angle.hpp"

#include "bvh/BvhBuilder.hpp"

#include "AlignedAllocator.hpp"

#include <utility>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

namespace SolidAngleBvhParams {
    static CONSTEXPR float InitialCoverageThreshold = 0.5f;
    static CONSTEXPR float InitialThreshold = 1.0f - (1.0f - InitialCoverageThreshold)*(1.0f - InitialCoverageThreshold);
    static CONSTEXPR float SubdivisionFactor = 0.5f;
}

class SolidAngleBvh
{
    struct Node {
        Vec3f point;
        float radiusSq;
        uint32 children;
        float cumulativeEmission;
        uint32 parent;
        uint32 padding; // Pad size to 32 bytes
    };

    template<typename T> using aligned_vector = std::vector<T, AlignedAllocator<T, 4096>>;

    aligned_vector<Node> _nodes;
    std::vector<uint32> _primToNode;

    float recursiveBuild(const Bvh::NaiveBvhNode *node, uint32 head, uint32 &tail,
            const std::vector<float> &emission)
    {
        if (node->isLeaf()) {
            _nodes[head].point = node->bbox().center();
            _nodes[head].radiusSq = node->bbox().diagonal().length()*0.5f;
            _nodes[head].children = node->id() | 0x80000000u;
            _nodes[head].cumulativeEmission = emission[node->id()];
            _primToNode[node->id()] = head;
        } else {
            _nodes[head].children = tail;
            tail += 2;

            float  leftSum = recursiveBuild(node->child(0), _nodes[head].children + 0, tail, emission);
            float rightSum = recursiveBuild(node->child(1), _nodes[head].children + 1, tail, emission);

            Node &lNode = _nodes[_nodes[head].children + 0];
            Node &rNode = _nodes[_nodes[head].children + 1];

            lNode.parent = head;
            rNode.parent = head;

            Vec3f pL = lNode.point;
            Vec3f pR = rNode.point;
            float rL = lNode.radiusSq;
            float rR = rNode.radiusSq;

            Vec3f d = pR - pL;
            float dist = d.length();
            if (dist < 1e-4f) {
                _nodes[head].point = pL;
                _nodes[head].radiusSq = rL;
            } else {
                Vec3f p0 = pL - max(rL, rR - dist)*d/dist;
                Vec3f p1 = pR + max(rR, rL - dist)*d/dist;
                _nodes[head].point = (p0 + p1)*0.5f;
                _nodes[head].radiusSq = (p1 - p0).length()*0.5f;
            }

            _nodes[head].cumulativeEmission = leftSum + rightSum;
        }

        return _nodes[head].cumulativeEmission;
    }

    template<typename InternalHandler, typename LeafHandler>
    void traverse(const Vec3f &p, float threshold, uint32 node, InternalHandler internalHandler,
            LeafHandler leafHandler) const
    {
        uint32 stack[32];
        uint32 *stackPtr = stack;

        while (true) {
            uint32 children = _nodes[node].children;

            if (children & 0x80000000u) {
                leafHandler(children);
            } else {
                float dSqL = (_nodes[children + 0].point - p).lengthSq();
                float dSqR = (_nodes[children + 1].point - p).lengthSq();

                float factorL = _nodes[children + 0].radiusSq/dSqL;
                float factorR = _nodes[children + 1].radiusSq/dSqR;

                bool traverseL = factorL >= threshold;
                bool traverseR = factorR >= threshold;

                if (traverseL && traverseR) {
                    *stackPtr++ = children;
                    node = children + 1;
                } else if (traverseL) {
                    internalHandler(_nodes[children + 1].cumulativeEmission/dSqR, children + 1);
                    node = children;
                } else if (traverseR) {
                    internalHandler(_nodes[children + 0].cumulativeEmission/dSqL, children + 0);
                    node = children + 1;
                } else {
                    internalHandler(_nodes[children + 0].cumulativeEmission/dSqL, children + 0);
                    internalHandler(_nodes[children + 1].cumulativeEmission/dSqR, children + 1);
                    goto pop;
                }
                continue;
            }

        pop:
            if (stackPtr == stack)
                break;
            node = *--stackPtr;
        }
    }

public:
    SolidAngleBvh(Bvh::PrimVector prims, std::vector<float> emission)
    {
        if (prims.empty()) {
            _nodes.emplace_back();
        } else {
            Bvh::BvhBuilder builder(2);
            builder.build(std::move(prims));

            _nodes.resize(builder.numNodes());
            _primToNode.resize(emission.size());

            uint32 tail = 1;
            recursiveBuild(builder.root().get(), 0, tail, emission);

            for (Node &node : _nodes)
                node.radiusSq *= node.radiusSq;

            _nodes[0].parent = 0;
        }
    }

    template<typename LeafWeight>
    inline float approximateContribution(const Vec3f &p, LeafWeight leafWeight) const
    {
        float result = 0.0f;
        traverse(p, SolidAngleBvhParams::InitialThreshold, 0, [&](float weight, uint32 /*id*/) {
            result += weight;
        }, [&](uint32 id) {
            result += leafWeight(id & 0x7FFFFFFFu);
        });

        return result;
    }

    template<typename LeafWeight>
    inline float lightPdf(const Vec3f &p, uint32 prim, LeafWeight leafWeight) const
    {
        uint32 stack[32];
        uint32 stackIndex = 0;
        uint32 node = _primToNode[prim];
        while (_nodes[node].parent != node) {
            stack[stackIndex++] = node;
            node = _nodes[node].parent;
        }

        float pdf = 1.0f;

        float coverageThreshold = SolidAngleBvhParams::InitialCoverageThreshold;
        float threshold = SolidAngleBvhParams::InitialThreshold;

        prim |= 0x80000000u;
        node = 0;
        while (prim != node) {
            float totalWeight = 0.0f, specificWeight = 0.0f;
            traverse(p, threshold, node, [&](float weight, uint32 id) {
                totalWeight += weight;
                for (uint32 i = 0; i < stackIndex; ++i) {
                    if (id == stack[i]) {
                        specificWeight = weight;
                        node = stack[i];
                    }
                }
            }, [&](uint32 id) {
                float weight = leafWeight(id & 0x7FFFFFFFu);
                totalWeight += weight;
                if (id == prim) {
                    specificWeight = weight;
                    node = prim;
                }
            });

            if (totalWeight == 0.0f)
                return 0.0f;

            pdf *= specificWeight/totalWeight;
            coverageThreshold *= SolidAngleBvhParams::SubdivisionFactor;
            threshold = 1.0f - sqr(1.0f - coverageThreshold);
        }

        return pdf;
    }

    template<typename LeafWeight>
    inline std::pair<int, float> sampleLight(const Vec3f &p, float *cdf, int *ids, float xi,
            LeafWeight leafWeight) const
    {
        float coverageThreshold = SolidAngleBvhParams::InitialCoverageThreshold;
        float threshold = SolidAngleBvhParams::InitialThreshold;

        int sampleIndex = 1;
        cdf[0] = 0.0f;

        float pdf = 1.0f;
        uint32 node = 0;
        while (true) {
            traverse(p, threshold, node, [&](float weight, uint32 id) {
                ids[sampleIndex] = id;
                cdf[sampleIndex] = cdf[sampleIndex - 1] + weight;
                sampleIndex++;
            }, [&](uint32 id) {
                ids[sampleIndex] = id;
                cdf[sampleIndex] = cdf[sampleIndex - 1] + leafWeight(id & 0x7FFFFFFFu);
                sampleIndex++;
            });

            if (cdf[sampleIndex - 1] == 0.0f)
                return std::make_pair(-1, 0.0f);

            int idx = sampleIndex - 1;
            float P = xi*cdf[sampleIndex - 1];
            for (int i = 1; i < sampleIndex; ++i) {
                if (cdf[i] > P) {
                    idx = i;
                    break;
                }
            }
            float weight = cdf[idx] - cdf[idx - 1];
            xi = clamp((P - cdf[idx - 1])/weight, 0.0f, 1.0f);
            pdf *= weight/cdf[sampleIndex - 1];

            if (ids[idx] & 0x80000000u)
                return std::make_pair(ids[idx] & 0x7FFFFFFFu, pdf);

            sampleIndex = 1;
            coverageThreshold *= SolidAngleBvhParams::SubdivisionFactor;
            threshold = 1.0f - sqr(1.0f - coverageThreshold);
            node = ids[idx];
        }

        return std::pair<int, float>(-1, 0.0f);
    }
};

}
}

#endif /* SOLIDANGLEBVH_HPP_ */
