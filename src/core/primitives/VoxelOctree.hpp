#ifndef VOXELOCTREE_HPP_
#define VOXELOCTREE_HPP_

#include "math/Vec.hpp"
#include "math/Ray.hpp"

#include <memory>
#include <limits>
#include <array>

namespace Tungsten {

template<int NumLevels, typename ElementType>
class VoxelOctree
{
    Vec3f _offset;
    std::array<std::unique_ptr<ElementType[]>, NumLevels> _grids;

    void buildHierarchy(int level, const ElementType *data, ElementType *parent)
    {
        const int size = 1 << (NumLevels - level);
        const int parentSize = size/2;

        auto brickContainsVoxels = [&](int bx, int by, int bz) {
            for (int z = bz*2; z < (bz + 1)*2; ++z)
                for (int y = by*2; y < (by + 1)*2; ++y)
                    for (int x = bx*2; x < (bx + 1)*2; ++x)
                        if (data[x + size*y + size*size*z])
                            return true;
            return false;
        };
        auto copyContainsVoxels = [&](int bx, int by, int bz, ElementType *dst) {
            bool contains = false;
            int base = (bx + size*by + size*size*bz)*2;

            for (int z = 0; z < 2; ++z)
                for (int y = 0; y < 2; ++y)
                    for (int x = 0; x < 2; ++x)
                        if ((dst[x + y*2 + z*4] = data[base + x + size*y + size*size*z]))
                            contains = true;
            return contains;
        };

        int nonZeroCount = 0;
        for (int z = 0; z < parentSize; ++z)
            for (int y = 0; y < parentSize; ++y)
                for (int x = 0; x < parentSize; ++x)
                    if (brickContainsVoxels(x, y, z))
                        nonZeroCount++;

        _grids[level].reset(new ElementType[nonZeroCount*8]);

        std::memset(parent, 0, sizeof(ElementType)*parentSize*parentSize*parentSize);

        int count = 0;
        for (int z = 0; z < parentSize && count < nonZeroCount; ++z) {
            for (int y = 0; y < parentSize && count < nonZeroCount; ++y) {
                for (int x = 0; x < parentSize && count < nonZeroCount; ++x) {
                    if (copyContainsVoxels(x, y, z, &_grids[level][count*8])) {
                        parent[x + parentSize*y + parentSize*parentSize*z] = count*8 + 1;
                        count++;
                    }
                }
            }
        }
    }

    template<typename Visitor>
    void iterateNonZeroVoxels(Visitor visitor, int level, int idx, int bx, int by, int bz)
    {
        for (int z = 0; z < 2; ++z) {
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 2; ++x) {
                    ElementType &value = _grids[level][idx + x + y*2 + z*4];
                    if (!value)
                        continue;

                    if (level)
                        iterateNonZeroVoxels(visitor, level - 1, value - 1, (bx + x)*2,
                                (by + y)*2, (bz + z)*2);
                    else
                        visitor(value, bx + x, by + y, bz + z);
                }
            }
        }
    }

public:
    VoxelOctree(Vec3f offset, const ElementType *data)
    : _offset(offset)
    {
        int temporarySize = 1 << 3*(NumLevels - 1);
        std::unique_ptr<ElementType[]> temporaryA(new ElementType[temporarySize]);
        std::unique_ptr<ElementType[]> temporaryB(new ElementType[temporarySize]);

        ElementType *bufferA = temporaryA.get(), *bufferB = temporaryB.get();

        _grids[NumLevels - 1].reset(new ElementType[8]);

        for (int i = 0; i < NumLevels - 1; ++i) {
            const ElementType *src = (i == 0 ? data : bufferA);
            ElementType *dst = (i == NumLevels - 2 ? _grids[NumLevels - 1].get() : bufferB);
            buildHierarchy(i, src, dst);
            std::swap(bufferA, bufferB);
        }
    }

    template<typename IntersectLambda>
    bool trace(Ray &ray, const Vec3f &/*dT*/, float tMin, IntersectLambda intersect) {
        CONSTEXPR int MaxScale = 23;
        CONSTEXPR int ScaleOffset = MaxScale - NumLevels;

        struct StackEntry
        {
            uint32 parent;
            float maxT;
        };
        StackEntry rayStack[MaxScale + 1];

        // Transform to [1, 2]^3 cube
        Vec3f o = (ray.pos() - _offset)*(1.0f/(1 << NumLevels)) + 1.0f;
        Vec3f d = ray.dir()*(1.0f/(1 << NumLevels));

        if (std::abs(d.x()) < 1e-8f) d.x() = 1e-8f;
        if (std::abs(d.y()) < 1e-8f) d.y() = 1e-8f;
        if (std::abs(d.z()) < 1e-8f) d.z() = 1e-8f;

        Vec3f dT = 1.0f/-std::abs(d);
        Vec3f bT = dT*o;

        uint32 octantMask = 0;
        if (d.x() > 0.0f) octantMask ^= 1, bT.x() = 3.0f*dT.x() - bT.x();
        if (d.y() > 0.0f) octantMask ^= 2, bT.y() = 3.0f*dT.y() - bT.y();
        if (d.z() > 0.0f) octantMask ^= 4, bT.z() = 3.0f*dT.z() - bT.z();

        float minT = max((2.0f*dT - bT).max(), tMin);
        float maxT = min((dT - bT).min(), ray.farT());

        uint32 parent  = 0;
        int idx     = 0;
        Vec3f pos(1.0f);
        int scale   = MaxScale - 1;
        float scaleExp2 = 0.5f;

        if (1.5f*dT.x() - bT.x() > minT) idx ^= 1, pos.x() = 1.5f;
        if (1.5f*dT.y() - bT.y() > minT) idx ^= 2, pos.y() = 1.5f;
        if (1.5f*dT.z() - bT.z() > minT) idx ^= 4, pos.z() = 1.5f;

        while (scale < MaxScale) {
            Vec3f cornerT = pos*dT - bT;
            float maxTC = cornerT.min();

            if (minT <= maxT) {
                float maxTV = min(maxT, maxTC);
                float half = scaleExp2*0.5f;
                Vec3f centerT = half*dT + cornerT;

                if (minT <= maxTV) {
                    uint32 childIdx = _grids[scale - ScaleOffset][parent + (idx ^ octantMask)];

                    if (childIdx) {
                        if (scale - ScaleOffset == 0) {
                            Vec3f p;
                            p[0] = (octantMask & 1) ? (2.0f - 1.0f/(1 << NumLevels)) - pos[0] : pos[0] - 1.0f;
                            p[1] = (octantMask & 2) ? (2.0f - 1.0f/(1 << NumLevels)) - pos[1] : pos[1] - 1.0f;
                            p[2] = (octantMask & 4) ? (2.0f - 1.0f/(1 << NumLevels)) - pos[2] : pos[2] - 1.0f;
                            if (intersect(childIdx - 1, _offset + p*float(1 << NumLevels), minT))
                                return true;
                        } else {
                            rayStack[scale] = StackEntry{parent, maxT};

                            parent = childIdx - 1;

                            idx = 0;
                            scale--;
                            scaleExp2 = half;

                            if (centerT.x() > minT) idx ^= 1, pos.x() += scaleExp2;
                            if (centerT.y() > minT) idx ^= 2, pos.y() += scaleExp2;
                            if (centerT.z() > minT) idx ^= 4, pos.z() += scaleExp2;

                            maxT = maxTV;

                            continue;
                        }
                    }
                }
            }

            int stepMask = 0;
            if (cornerT.x() <= maxTC) stepMask ^= 1, pos.x() -= scaleExp2;
            if (cornerT.y() <= maxTC) stepMask ^= 2, pos.y() -= scaleExp2;
            if (cornerT.z() <= maxTC) stepMask ^= 4, pos.z() -= scaleExp2;

            minT = maxTC;
            idx ^= stepMask;

            if ((idx & stepMask) != 0) {
                int differingBits = 0;
                if (stepMask & 1) differingBits |= BitManip::floatBitsToUint(pos.x()) ^ BitManip::floatBitsToUint(pos.x() + scaleExp2);
                if (stepMask & 2) differingBits |= BitManip::floatBitsToUint(pos.y()) ^ BitManip::floatBitsToUint(pos.y() + scaleExp2);
                if (stepMask & 4) differingBits |= BitManip::floatBitsToUint(pos.z()) ^ BitManip::floatBitsToUint(pos.z() + scaleExp2);
                scale = (BitManip::floatBitsToUint((float)differingBits) >> 23) - 127;
                scaleExp2 = BitManip::uintBitsToFloat((scale - MaxScale + 127) << 23);

                parent = rayStack[scale].parent;
                maxT   = rayStack[scale].maxT;

                int shX = BitManip::floatBitsToUint(pos.x()) >> scale;
                int shY = BitManip::floatBitsToUint(pos.y()) >> scale;
                int shZ = BitManip::floatBitsToUint(pos.z()) >> scale;
                pos.x() = BitManip::uintBitsToFloat(shX << scale);
                pos.y() = BitManip::uintBitsToFloat(shY << scale);
                pos.z() = BitManip::uintBitsToFloat(shZ << scale);
                idx = (shX & 1) | ((shY & 1) << 1) | ((shZ & 1) << 2);
            }
        }
        return false;
    }

    ElementType *at(int x, int y, int z)
    {
        int idx = 0;
        for (int i = NumLevels - 1; i > 0; --i) {
            int px = (x >> i) & 1;
            int py = (y >> i) & 1;
            int pz = (z >> i) & 1;

            idx = _grids[i][idx + px + py*2 + pz*4];
            if (!idx)
                return nullptr;
            idx--;
        }

        return &_grids[0][idx + (x & 1) + (y & 1)*2 + (z & 1)*4];
    }

    template<typename Visitor>
    void iterateNonZeroVoxels(Visitor visitor)
    {
        iterateNonZeroVoxels(visitor, NumLevels - 1, 0, 0, 0, 0);
    }
};

}

#endif /* VOXELOCTREE_HPP_ */
