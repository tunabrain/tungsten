#ifndef FRUSTUMBINNER_HPP_
#define FRUSTUMBINNER_HPP_

#include "cameras/Camera.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"
#include "math/Box.hpp"

#include "sse/SimdFloat.hpp"

#include "IntTypes.hpp"

#include <array>

namespace Tungsten {

class FrustumBinner
{
    static CONSTEXPR uint32 TileSize = 4;
    static CONSTEXPR uint32 TileMask = TileSize - 1;

    static inline float minReduce(float4 a, float4 b, float4 c)
    {
        float4 reduce = min(min(a, b), c);
        return min(min(reduce[0], reduce[1]), min(reduce[2], reduce[3]));
    }

    struct QuadSetup
    {
        Box2i bounds;
        float4 stepX[3];
        float4 stepY[3];
        float4 offset[3];
        float4 wy[3];
        float4 wx[3];

        inline void start(float x, float y)
        {
            float4 xf = float4(x);
            float4 yf = float4(y);
            wy[0] = stepX[0]*xf + stepY[0]*yf + offset[0];
            wy[1] = stepX[1]*xf + stepY[1]*yf + offset[1];
            wy[2] = stepX[2]*xf + stepY[2]*yf + offset[2];
        }
        inline void beginRow() { wx[0]  =    wy[0]; wx[1]  =    wy[1]; wx[2]  =    wy[2]; }
        inline void endRow()   { wy[0] += stepY[0]; wy[1] += stepY[1]; wy[2] += stepY[2]; }
        inline void stepCol()  { wx[0] += stepX[0]; wx[1] += stepX[1]; wx[2] += stepX[2]; }
        inline float reduce() { return minReduce(wx[0], wx[1], wx[2]); }
    };

    float _guardBand;
    Vec2u _res;
    float _aspect;
    float _f;
    Vec2f _scale;
    Mat4f _invT;
    Vec3f _pos;

    QuadSetup setupQuad(Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3) const;

public:
    FrustumBinner(const Camera &camera);

    template<size_t N, typename Intersector>
    void iterateTiles(std::array<QuadSetup, N> quads, Intersector intersector) const
    {
        Box2i bounds;
        for (const auto &q : quads)
            bounds.grow(q.bounds);

        if (bounds.empty())
            return;

        uint32 minX = uint32(bounds.min().x()) & ~TileMask, maxX = bounds.max().x();
        uint32 minY = uint32(bounds.min().y()) & ~TileMask, maxY = bounds.max().y();

        for (auto &q : quads) {
            q.start(minX + TileSize*0.5f, minY + TileSize*0.5f);
            for (int k = 0; k < 3; ++k) {
                q.stepX[k] *= float(TileSize);
                q.stepY[k] *= float(TileSize);
            }
        }

        for (uint32 y = minY; y < maxY; y += TileSize) {
            for (auto &q : quads)
                q.beginRow();

            for (uint32 x = minX; x < maxX; x += TileSize) {
                float wMin = -1.0f;
                for (auto &q : quads)
                    wMin = max(wMin, q.reduce());
                if (wMin >= 0.0f) {
                    uint32 xjMax = min(x + TileSize, _res.x());
                    uint32 yjMax = min(y + TileSize, _res.y());
                    for (uint32 yj = y; yj < yjMax; ++yj)
                        for (uint32 xj = x; xj < xjMax; ++xj)
                            intersector(xj, yj, xj + yj*_res.x());
                }
                for (auto &q : quads)
                    q.stepCol();
            }
            for (auto &q : quads)
                q.endRow();
        }
    }

    template<typename Intersector>
    void binBeam(Vec3f b0, Vec3f b1, Vec3f u, float radius, Intersector intersector) const
    {
        Vec3f p0 = _invT*(b0 - u*radius);
        Vec3f p1 = _invT*(b0 + u*radius);
        Vec3f p2 = _invT*(b1 + u*radius);
        Vec3f p3 = _invT*(b1 - u*radius);

        std::array<QuadSetup, 1> quads{{setupQuad(p0, p1, p2, p3)}};

        iterateTiles(quads, intersector);
    }

    template<typename Intersector>
    void binPlane(Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3, Intersector intersector) const
    {
        std::array<QuadSetup, 1> quads{{setupQuad(_invT*p0, _invT*p1, _invT*p2, _invT*p3)}};

        iterateTiles(quads, intersector);
    }

    template<typename Intersector>
    void binPlane1D(Vec3f center, Vec3f a, Vec3f b, Vec3f c, Intersector intersector) const
    {
        center = _invT*center;
        a = _invT.transformVector(a);
        b = _invT.transformVector(b);
        c = _invT.transformVector(c);

        std::array<QuadSetup, 6> quads{{
            setupQuad(center + c - a - b, center + c - a + b, center + c + a + b, center + c + a - b),
            setupQuad(center - c - a - b, center - c - a + b, center - c + a + b, center - c + a - b),
            setupQuad(center + b - a - c, center + b - a + c, center + b + a + c, center + b + a - c),
            setupQuad(center - b - a - c, center - b - a + c, center - b + a + c, center - b + a - c),
            setupQuad(center + a - b - c, center + a - b + c, center + a + b + c, center + a + b - c),
            setupQuad(center - a - b - c, center - a - b + c, center - a + b + c, center - a + b - c)
        }};

        iterateTiles(quads, intersector);
    }
};

}

#endif /* FRUSTUMBINNER_HPP_ */
