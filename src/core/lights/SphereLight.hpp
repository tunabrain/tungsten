#ifndef SPHERELIGHT_HPP_
#define SPHERELIGHT_HPP_

#include "Light.hpp"

#include "math/TangentSpace.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class SphereLight : public Light
{
    static constexpr float LargeT = 1000.0f;

    float _radius;
    Vec3f _pos;
    Vec3f _emission;

public:
    SphereLight(const Vec3f &emission)
    : _emission(emission)
    {
    }

    SphereLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _emission(JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "sphere", allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _pos = _transform*Vec3f(0.0f);
        _radius = (_transform.extractScale()*Vec3f(1.0f)).max();
    }

    virtual void buildProxy() final override
    {
        std::vector<Vertex> verts;
        std::vector<TriangleI> tris;

        constexpr int SubDiv = 10;
        constexpr int Skip = SubDiv*2 + 1;
        for (int f = 0, idx = 0; f < 3; ++f) {
            for (int s = -1; s <= 1; s += 2) {
                for (int u = -SubDiv; u <= SubDiv; ++u) {
                    for (int v = -SubDiv; v <= SubDiv; ++v, ++idx) {
                        Vec3f p(0.0f);
                        p[f] = s;
                        p[(f + 1) % 3] = u*(1.0f/SubDiv)*s;
                        p[(f + 2) % 3] = v*(1.0f/SubDiv);
                        verts.emplace_back(p.normalized());

                        if (v > -SubDiv && u > -SubDiv) {
                            tris.emplace_back(idx - Skip - 1, idx - Skip, idx);
                            tris.emplace_back(idx - Skip - 1, idx, idx - 1);
                        }
                    }
                }
            }
        }

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "SphereLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<SphereLight>(*this);
    }

    bool isDelta() const override final
    {
        return false;
    }

    bool intersect(const Vec3f &p, const Vec3f &w, float &t, Vec3f &q) const override final
    {
        Vec3f o = p - _pos;
        float B = o.dot(w);
        float BSq = B*B;
        float C = o.lengthSq() - _radius*_radius;

        if (BSq < C)
            return false;

        t = -B - std::sqrt(BSq - C);
        q = p + w*t;
        return true;
    }

    bool sample(LightSample &sample) const override final
    {
        Vec3f w = _pos - sample.p;
        float r = w.length();
        float h = std::sqrt(r*r + _radius*_radius);
        float cosApex = r/h;

        float phi      = sample.xi.x()*TWO_PI;
        float cosTheta = sample.xi.y()*(1.0f - cosApex) + cosApex;
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f wLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentSpace frame(w/r);
        sample.w = frame.toGlobal(wLocal);
        intersect(sample.p, sample.w, sample.r, sample.q);
        sample.pdf = INV_PI/(1.0f - cosApex);
        sample.L = _emission;
        return true;
    }

    Vec3f eval(const Vec3f &/*w*/) const override final
    {
        return _emission;
    }

    float solidAngle(const Vec3f &p) const
    {
        Vec3f w = _pos - p;
        float rSq = w.lengthSq();
        float hSq = rSq + _radius*_radius;
        float cosApex = std::sqrt(rSq/hSq);

        return TWO_PI*(1.0f - cosApex);
    }

    float pdf(const Vec3f &p, const Vec3f &/*n*/, const Vec3f &/*w*/) const override final
    {
        return 1.0f/solidAngle(p);
    }

    Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &/*n*/) const override final
    {
        return approximateRadiance(p);
    }

    Vec3f approximateRadiance(const Vec3f &p) const override final
    {
        return _emission*solidAngle(p);
    }
};

}


#endif /* SPHERELIGHT_HPP_ */
