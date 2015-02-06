#ifndef QUADGEOMETRY_HPP_
#define QUADGEOMETRY_HPP_

#include "Triangle4.hpp"

#include "mc-loader/TexturedQuad.hpp"

#include "math/Mat4f.hpp"
#include "math/Box.hpp"

#include "AlignedAllocator.hpp"

#include <utility>
#include <vector>

namespace Tungsten {

class QuadGeometry
{
public:
    struct Intersection
    {
        float u, v;
        uint32 id;
    };

    struct TriangleInfo
    {
        Vec3f Ng;
        Vec3f p0, p1, p2;
        Vec2f uv0, uv1, uv2;
        int material;
    };

private:
    template<typename T> using aligned_vector = std::vector<T, AlignedAllocator<T, 4096>>;


    aligned_vector<Triangle4> _geometry;
    std::vector<TriangleInfo> _triInfo;
    std::vector<std::pair<int, int>> _simdSpan, _modelSpan;
    int _triangleOffset;

public:
    QuadGeometry() = default;

    void beginModel()
    {
        _simdSpan.emplace_back(_geometry.size(), _geometry.size());
        _modelSpan.emplace_back(_triInfo.size(), _triInfo.size());
        _triangleOffset = 0;
    }

    void addQuad(const TexturedQuad &quad, int material, const Mat4f &transform)
    {
        Vec2f uv0(quad.uv0.x(), 1.0f - quad.uv0.y());
        Vec2f uv1(quad.uv1.x(), 1.0f - quad.uv1.y());
        Vec2f uv2(quad.uv2.x(), 1.0f - quad.uv2.y());
        Vec2f uv3(quad.uv3.x(), 1.0f - quad.uv3.y());
        Vec3f p0 = transform*quad.p0;
        Vec3f p1 = transform*quad.p1;
        Vec3f p2 = transform*quad.p2;
        Vec3f p3 = transform*quad.p3;

        Vec3f Ng = ((p2 - p0).cross(p1 - p0)).normalized();

        _triInfo.emplace_back(TriangleInfo{Ng, p0, p2, p1, uv0, uv2, uv1, material});
        _triInfo.emplace_back(TriangleInfo{Ng, p3, p2, p0, uv3, uv2, uv0, material});

        if (_triangleOffset == 0)
            _geometry.emplace_back();

        _geometry.back().set(_triangleOffset++, p0, p2, p1, _triInfo.size() - 2);
        _geometry.back().set(_triangleOffset++, p3, p2, p0, _triInfo.size() - 1);

        _triangleOffset %= 4;
    }

    void endModel()
    {
        _simdSpan.back().second = _geometry.size();
        _modelSpan.back().second = _triInfo.size();

        if (_triangleOffset > 0)
            for (int i = 3; i >= _triangleOffset; --i)
                _geometry.back().set(i, Vec3f(0.0f), Vec3f(0.0f), Vec3f(0.0f), 0);
    }

    void addQuads(const QuadGeometry &o, int idx, const Mat4f &transform)
    {
        int simdStart  = o. _simdSpan[idx].first;
        int simdEnd    = o. _simdSpan[idx].second;
        int modelStart = o._modelSpan[idx].first;
        int modelEnd   = o._modelSpan[idx].second;

        _simdSpan.emplace_back(_geometry.size(), _geometry.size() + simdEnd - simdStart);
        _modelSpan.emplace_back(_triInfo.size(), _triInfo.size() + modelEnd - modelStart);

        int infoBase = _triInfo.size();
        for (int i = simdStart; i < simdEnd; ++i) {
            _geometry.emplace_back(o._geometry[i]);

            Triangle4 &t = _geometry.back();
            for (int j = 0; j < 4; ++j) {
                Vec3f p0, p1, p2;
                uint32 id;
                t.get(j, p0, p1, p2, id);
                t.set(j, transform*p0, transform*p1, transform*p2, infoBase++);
            }
        }

        for (int i = modelStart; i < modelEnd; ++i) {
            _triInfo.emplace_back(o._triInfo[i]);
            _triInfo.back().p0 = transform*_triInfo.back().p0;
            _triInfo.back().p1 = transform*_triInfo.back().p1;
            _triInfo.back().p2 = transform*_triInfo.back().p2;
        }
    }

    inline void intersect(Ray &ray, int idx, Intersection &isect) const
    {
        int start = _simdSpan[idx].first;
        int end   = _simdSpan[idx].second;

        for (int i = start; i < end; ++i)
            intersectTriangle4(ray, _geometry[i], isect.u, isect.v, isect.id);
    }

    Box3f bounds(int idx) const
    {
        Box3f bounds;

        int start = _modelSpan[idx].first;
        int end   = _modelSpan[idx].second;

        for (int i = start; i < end; i += 2) {
            bounds.grow(_triInfo[i].p0);
            bounds.grow(_triInfo[i].p1);
            bounds.grow(_triInfo[i].p2);
            bounds.grow(_triInfo[i + 1].p0);
        }

        return bounds;
    }

    inline const TriangleInfo &triangle(int i) const
    {
        return _triInfo[i];
    }

    inline Vec3f normal(const Intersection &isect) const
    {
        return _triInfo[isect.id].Ng;
    }

    inline Vec2f uv(const Intersection &isect) const
    {
        Vec2f uv0 = _triInfo[isect.id].uv0;
        Vec2f uv1 = _triInfo[isect.id].uv1;
        Vec2f uv2 = _triInfo[isect.id].uv2;
        return (1.0f - isect.u - isect.v)*uv0 + isect.u*uv1 + isect.v*uv2;
    }

    inline int material(const Intersection &isect) const
    {
        return _triInfo[isect.id].material;
    }

    inline int size() const
    {
        return _modelSpan.size();
    }

    inline int triangleCount() const
    {
        return _triInfo.size();
    }

    inline bool nonEmpty(int idx) const
    {
        return _modelSpan[idx].first != _modelSpan[idx].second;
    }
};

}

#endif /* QUADGEOMETRY_HPP_ */
