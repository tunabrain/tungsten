#ifndef BVHBUILDER_HPP_
#define BVHBUILDER_HPP_

#include "NaiveBvhNode.hpp"
#include "Primitive.hpp"

namespace Tungsten {

namespace Bvh {

class BvhBuilder
{
    std::unique_ptr<NaiveBvhNode> _root;
    uint32 _depth;
    uint32 _numNodes;
    uint32 _branchFactor;

public:
    BvhBuilder(uint32 branchFactor);

    void build(PrimVector prims);
    void integrityCheck(const NaiveBvhNode &node, int depth) const;

    std::unique_ptr<NaiveBvhNode> &root()
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
