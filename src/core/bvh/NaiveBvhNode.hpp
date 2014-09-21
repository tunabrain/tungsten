#ifndef NAIVEBVHNODE_HPP_
#define NAIVEBVHNODE_HPP_

#include "math/Box.hpp"

#include <memory>
#include <array>

namespace Tungsten {

namespace Bvh {

class NaiveBvhNode
{
    std::array<std::unique_ptr<NaiveBvhNode>, 4> _children;
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
};

}

}



#endif /* NAIVEBVHNODE_HPP_ */
