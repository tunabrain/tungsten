#ifndef EMISSIVEBVH_HPP_
#define EMISSIVEBVH_HPP_

#include "bvh/BvhBuilder.hpp"

#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class EmissiveBvh
{
    struct Node {
        Box3f bounds;
        uint32 children;
        float cumulativeEmission;
    };

    std::vector<Node> _nodes;

    float recursiveBuild(const Bvh::NaiveBvhNode *node, uint32 head, uint32 &tail,
            const std::vector<float> &emission)
    {
        _nodes[head].bounds = node->bbox();

        if (node->isLeaf()) {
            _nodes[head].children = node->id() | 0x80000000u;
            _nodes[head].cumulativeEmission = emission[node->id()];
        } else {
            _nodes[head].children = tail;
            tail += 2;

            float  leftSum = recursiveBuild(node->child(0), _nodes[head].children + 0, tail, emission);
            float rightSum = recursiveBuild(node->child(1), _nodes[head].children + 1, tail, emission);

            _nodes[head].cumulativeEmission = leftSum + rightSum;
        }

        return _nodes[head].cumulativeEmission;
    }

public:
    EmissiveBvh(Bvh::PrimVector prims, std::vector<float> emission)
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
    inline float traverse(const Vec3f &p, LAMBDA traverser) const
    {
        float totalOutside = 0.0f;

        if (!_nodes[0].bounds.contains(p))
            return _nodes[0].cumulativeEmission;

        uint32 node = 0;
        uint32 stack[32];
        uint32 *stackPtr = stack;

        while (true) {
            uint32 children = _nodes[node].children;

            if (children & 0x80000000u) {
                traverser(children & 0x7FFFFFFFu);
            } else {
                bool containsL = _nodes[children + 0].bounds.contains(p);
                bool containsR = _nodes[children + 1].bounds.contains(p);

                if (containsL && containsR) {
                    *stackPtr++ = children;
                    node = children + 1;
                } else if (containsL) {
                    totalOutside += _nodes[children + 1].cumulativeEmission;
                    node = children;
                } else if (containsR) {
                    totalOutside += _nodes[children].cumulativeEmission;
                    node = children + 1;
                } else {
                    totalOutside = totalOutside
                        + _nodes[children + 0].cumulativeEmission
                        + _nodes[children + 1].cumulativeEmission;
                    goto pop;
                }
                continue;
            }

        pop:
            if (stackPtr == stack)
                break;
            node = *--stackPtr;
        }

        return totalOutside;
    }
};

}
}

#endif /* EMISSIVEBVH_HPP_ */
