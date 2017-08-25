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

struct PathPhoton
{
    Vec3f pos;
    Vec3f power;
    Vec3f dir;
    float length;
    float sampledLength;
    uint32 data;

    void setPathInfo(uint32 bounce, bool onSurface)
    {
        data = bounce;
        if (onSurface)
            data |= (1u << 31u);
    }
    bool onSurface() const
    {
        return (data & (1u << 31u)) != 0;
    }
    uint32 bounce() const
    {
        return data & ~(1u << 31u);
    }
};
struct PhotonBeam
{
    Vec3f p0, p1;
    Vec3f dir;
    float length;
    Vec3f power;
    int bounce;
    bool valid;
};
struct PhotonPlane0D
{
    Vec3f p0, p1, p2, p3;
    Vec3f power;
    Vec3f d1;
    float l1;
    int bounce;
    bool valid;

    Box3f bounds() const
    {
        Box3f box;
        box.grow(p0);
        box.grow(p1);
        box.grow(p2);
        box.grow(p3);
        return box;
    }
};
struct PhotonPlane1D
{
    Vec3f p;
    Vec3f invU, invV, invW;
    Vec3f center, a, b, c;
    Vec3f power;
    Vec3f d1;
    float l1;
    float invDet;
    float binCount;
    int bounce;
    bool valid;

    Box3f bounds() const
    {
        Box3f box;
        box.grow(center + a + b + c);
        box.grow(center - a + b + c);
        box.grow(center + a - b + c);
        box.grow(center - a - b + c);
        box.grow(center + a + b - c);
        box.grow(center - a + b - c);
        box.grow(center + a - b - c);
        box.grow(center - a - b - c);
        return box;
    }
};

}

#endif /* PHOTON_HPP_ */
