#ifndef TRIANGLE4_HPP_
#define TRIANGLE4_HPP_

#include "math/Vec.hpp"
#include "math/Ray.hpp"

#include "sse/SimdUtils.hpp"

#include <array>

namespace Tungsten {

struct Triangle4
{
    Vec<float4, 3> p0, p1, p2;
    std::array<uint32, 4> id;

    Triangle4() = default;

    Triangle4(const Vec<float4, 3> &p0_,
              const Vec<float4, 3> &p1_,
              const Vec<float4, 3> &p2_,
              std::array<uint32, 4> id_)
    : p0(p0_), p1(p1_), p2(p2_), id(id_)
    {
    }

    void set(uint32 k, const Vec3f &p0_, const Vec3f &p1_, const Vec3f &p2_, uint32 id_)
    {
        for (int i = 0; i < 3; ++i) {
            p0[i][k] = p0_[i];
            p1[i][k] = p1_[i];
            p2[i][k] = p2_[i];
        }
        id[k] = id_;
    }

    void get(uint32 k, Vec3f &p0_, Vec3f &p1_, Vec3f &p2_, uint32 &id_) const
    {
        for (int i = 0; i < 3; ++i) {
            p0_[i] = p0[i][k];
            p1_[i] = p1[i][k];
            p2_[i] = p2[i][k];
        }
        id_ = id[k];
    }
};

inline Vec<float4, 3> transpose(const Vec3f &p)
{
    return Vec<float4, 3>(float4(p.x()), float4(p.y()), float4(p.z()));
}

inline void intersectTriangle4(Ray &ray, const Triangle4 &t,
        float &resultU, float &resultV, uint32 &resultId)
{
    typedef SimdBool<4> pbool;

    Vec<float4, 3> rayD = transpose(ray.dir());
    Vec<float4, 3> rayO = transpose(ray.pos());

    Vec<float4, 3> e1 = t.p1 - t.p0;
    Vec<float4, 3> e2 = t.p2 - t.p0;
    Vec<float4, 3> P = rayD.cross(e2);
    float4 det = e1.dot(P);
    pbool invalidMask = (det <= float4(0.0f));
    //if (invalidMask.all()) return;
    float4 invDet = float4(1.0f)/det;

    Vec<float4, 3> T = rayO - t.p0;

    float4 u = T.dot(P)*invDet;
    invalidMask = invalidMask || (u < float4(0.0f)) || (u > float4(1.0f));
    //if (invalidMask.all()) return;

    Vec<float4, 3> Q = T.cross(e1);
    float4 v = rayD.dot(Q)*invDet;
    invalidMask = invalidMask || (v < float4(0.0f)) || (u + v > float4(1.0f));
    //if (invalidMask.all()) return;

    float4 maxT = e2.dot(Q)*invDet;
    invalidMask = invalidMask || (maxT <= float4(ray.nearT())) || (maxT >= float4(ray.farT()));
    //if (invalidMask.all()) return;
    maxT = maxT.blend(float4(ray.farT()), invalidMask);

    int minId = -1;
    float tMin = ray.farT();
    if (maxT[0] < tMin) { minId = 0; tMin = maxT[0]; }
    if (maxT[1] < tMin) { minId = 1; tMin = maxT[1]; }
    if (maxT[2] < tMin) { minId = 2; tMin = maxT[2]; }
    if (maxT[3] < tMin) { minId = 3; tMin = maxT[3]; }

    if (minId != -1) {
        ray.setFarT(tMin);
        resultU = u[minId];
        resultV = v[minId];
        resultId = t.id[minId];
    }
}

}

#endif /* TRIANGLE4_HPP_ */
