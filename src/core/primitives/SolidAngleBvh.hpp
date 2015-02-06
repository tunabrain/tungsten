#ifndef SOLIDANGLEBVH_HPP_
#define SOLIDANGLEBVH_HPP_

#include "math/Angle.hpp"

#include "bvh/BvhBuilder.hpp"

#include <utility>
#include <vector>

namespace Tungsten {

class SolidAngleBvh
{
    struct Node {
        Vec3f point;
        float radius;
        uint32 children;
        float cumulativeEmission;
    };

    std::vector<Node> _nodes;

    float recursiveBuild(const Bvh::NaiveBvhNode *node, uint32 head, uint32 &tail,
            const std::vector<float> &emission)
    {
        if (node->isLeaf()) {
            _nodes[head].point = node->bbox().center();
            _nodes[head].radius = node->bbox().diagonal().length()*0.5f;
            _nodes[head].children = node->id() | 0x80000000u;
            _nodes[head].cumulativeEmission = emission[node->id()];
        } else {
            _nodes[head].children = tail;
            tail += 2;

            float  leftSum = recursiveBuild(node->child(0), _nodes[head].children + 0, tail, emission);
            float rightSum = recursiveBuild(node->child(1), _nodes[head].children + 1, tail, emission);

            Vec3f pL = _nodes[_nodes[head].children + 0].point;
            Vec3f pR = _nodes[_nodes[head].children + 1].point;
            float rL = _nodes[_nodes[head].children + 0].radius;
            float rR = _nodes[_nodes[head].children + 1].radius;

            Vec3f d = pR - pL;
            float dist = d.length();
            if (dist < 1e-4f) {
                _nodes[head].point = pL;
                _nodes[head].radius = rL;
            } else {
                Vec3f p0 = pL - max(rL, rR - dist)*d/dist;
                Vec3f p1 = pR + max(rR, rL - dist)*d/dist;
                _nodes[head].point = (p0 + p1)*0.5f;
                _nodes[head].radius = (p1 - p0).length()*0.5f;
            }

            _nodes[head].cumulativeEmission = leftSum + rightSum;
        }

        return _nodes[head].cumulativeEmission;
    }

    inline float sphereSolidAngle(float dSq, float rSq) const
    {
        float cosTheta = std::sqrt(max(dSq - rSq, 0.0f)/dSq);

        return TWO_PI*(1.0f - cosTheta);
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

            uint32 tail = 1;
            recursiveBuild(builder.root().get(), 0, tail, emission);
        }
    }

    template<typename LAMBDA>
    inline std::pair<int, float> traverse(const Vec3f &p, float *cdf, int *ids, float xi, LAMBDA leafHandler) const
    {
        float solidangleThreshold = TWO_PI/10.0f;

        int sampleIndex = 0;
        auto addSample = [&](float weight, int id) {
            ids[sampleIndex] = id;
            cdf[sampleIndex] = weight;

            if (sampleIndex > 0)
                cdf[sampleIndex] += cdf[sampleIndex - 1];
            sampleIndex++;
        };

        float pdf = 1.0f;
        std::pair<int, float> result(-1, 0.0f);

        uint32 node = 0;
        uint32 stack[32];
        uint32 *stackPtr = stack;

        while (true) {
            while (true) {
                uint32 children = _nodes[node].children;

                if (children & 0x80000000u) {
                    addSample(leafHandler(children & 0x7FFFFFFFu), children);
                } else {
                    float dSqL = (_nodes[children + 0].point - p).lengthSq();
                    float dSqR = (_nodes[children + 1].point - p).lengthSq();

                    float angleL = sphereSolidAngle(dSqL, sqr(_nodes[children + 0].radius));
                    float angleR = sphereSolidAngle(dSqR, sqr(_nodes[children + 1].radius));

                    bool traverseL = angleL >= solidangleThreshold;
                    bool traverseR = angleR >= solidangleThreshold;

                    if (traverseL && traverseR) {
                        *stackPtr++ = children;
                        node = children + 1;
                    } else if (traverseL) {
                        addSample(_nodes[children + 1].cumulativeEmission/dSqR, children + 1);
                        node = children;
                    } else if (traverseR) {
                        addSample(_nodes[children + 0].cumulativeEmission/dSqL, children + 0);
                        node = children + 1;
                    } else {
                        addSample(_nodes[children + 0].cumulativeEmission/dSqL, children + 0);
                        addSample(_nodes[children + 1].cumulativeEmission/dSqR, children + 1);
                        goto pop;
                    }
                    continue;
                }

            pop:
                if (stackPtr == stack)
                    break;
                node = *--stackPtr;
            }

            int idx = sampleIndex - 1;
            float P = xi*cdf[sampleIndex - 1];
            for (int i = 0; i < sampleIndex; ++i) {
                if (cdf[i] > P) {
                    idx = i;
                    break;
                }
            }
            float lower = idx ? cdf[idx - 1] : 0.0f;
            float weight = cdf[idx] - lower;
            xi = clamp((P - lower)/weight, 0.0f, 1.0f);
            pdf *= weight/cdf[sampleIndex - 1];

            if (ids[idx] & 0x80000000u) {
                result = std::make_pair(ids[idx] & 0x7FFFFFFFu, pdf);
                break;
            }

            sampleIndex = 0;
            solidangleThreshold *= 0.1f;
            node = ids[idx];
        }

        return result;
    }
};

}

#endif /* SOLIDANGLEBVH_HPP_ */
