#ifndef PHOTON_HPP_
#define PHOTON_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

struct Photon
{
    uint32 splitData;
    uint32 bounce;
    Vec3f pos;
    Vec3f dir;
    Vec3f power;

    void setSplitInfo(uint32 childIdx, uint32 splitDim, uint32 childCount)
    {
        uint32 childMask = childCount == 0 ? 0 : (childCount == 1 ? 1 : 3);
        splitData = (splitDim << 30u) | (childMask << 28u) | childIdx;
    }

    bool hasLeftChild() const
    {
        return (splitData & (1u << 28u)) != 0;
    }

    bool hasRightChild() const
    {
        return (splitData & (1u << 29u)) != 0;
    }

    uint32 splitDim() const
    {
        return splitData >> 30u;
    }

    uint32 childIdx() const
    {
        return splitData & 0x0FFFFFFFu;
    }
};

struct VolumePhoton : public Photon
{
    Vec3f minBounds;
    Vec3f maxBounds;
    float radiusSq;
};

}



#endif /* PHOTON_HPP_ */
