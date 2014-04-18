#ifndef POINTLIGHT_HPP_
#define POINTLIGHT_HPP_

#include "Light.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class PointLight : public Light
{
    Vec3f _pos;
    Vec3f _emission;

public:
    PointLight(const Vec3f &emission)
    : _emission(emission)
    {
    }

    PointLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _emission(JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "point", allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _pos = _transform*Vec3f(0.0f);
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
                        p[(f + 1) % 3] = u*(1.0f/SubDiv);
                        p[(f + 2) % 3] = v*(1.0f/SubDiv);
                        verts.emplace_back(p.normalized()*0.01f);

                        if (v > -SubDiv && u > -SubDiv) {
                            tris.emplace_back(idx - Skip - 1, idx - Skip, idx);
                            tris.emplace_back(idx - Skip - 1, idx, idx - 1);
                        }
                    }
                }
            }
        }

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "PointLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<PointLight>(*this);
    }

    bool isDelta() const override final
    {
        return true;
    }

    bool intersect(const Vec3f &/*p*/, const Vec3f &/*w*/, float &/*t*/, Vec3f &/*q*/) const override final
    {
        return false;
    }

    bool sample(LightSample &sample) const override final
    {
        sample.q = _pos;
        sample.w = sample.q - sample.p;
        float rSq = sample.w.lengthSq();
        sample.r = std::sqrt(rSq);
        sample.w /= sample.r;
        sample.pdf = rSq;
        sample.L = _emission;
        return true;
    }

    Vec3f eval(const Vec3f &/*w*/) const override final
    {
        return _emission;
    }

    float pdf(const Vec3f &/*p*/, const Vec3f &/*n*/, const Vec3f &/*w*/) const override final
    {
        return 0.0f;
    }

    Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &n) const override final
    {
        Vec3f w = _pos - p;
        float r = w.length();
        return _emission*std::abs(w.dot(n))/(r*r*r);
    }

    Vec3f approximateRadiance(const Vec3f &p) const override final
    {
        return _emission/(p - _pos).lengthSq();
    }
};

}


#endif /* POINTLIGHT_HPP_ */
