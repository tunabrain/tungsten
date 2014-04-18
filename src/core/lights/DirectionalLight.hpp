#ifndef DIRECTIONALLIGHT_HPP_
#define DIRECTIONALLIGHT_HPP_

#include "Light.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class DirectionalLight : public Light
{
    static constexpr float LargeT = 100000.0f;

    Vec3f _dir;
    Vec3f _emission;

public:
    DirectionalLight(const Vec3f &emission)
    : _emission(emission)
    {
    }

    DirectionalLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _emission(JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "directional", allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _dir = _transform.transformVector(Vec3f(0.0f, -1.0f, 0.0f)).normalized();
    }

    virtual void buildProxy() final override
    {
        std::vector<Vertex> verts;
        std::vector<TriangleI> tris;

        float r = 0.05f;
        for (int i = 0; i < 360; ++i) {
            float a = Angle::degToRad(i);
            verts.emplace_back(Vec3f(std::cos(a)*r, 1.0f, std::sin(a)*r));
            tris.emplace_back(360, i, (i + 1) % 360);
            if (i > 1)
                tris.emplace_back(0, i - 1, i);
        }
        verts.emplace_back(Vec3f(0.0f));

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "DirectionalLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<DirectionalLight>(*this);
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
        sample.q = sample.p - _dir*LargeT;
        sample.w = -_dir;
        sample.r = LargeT;
        sample.pdf = 1.0f;
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

    Vec3f approximateIrradiance(const Vec3f &/*p*/, const Vec3f &n) const override final
    {
        return _emission*std::abs(_dir.dot(n));
    }

    Vec3f approximateRadiance(const Vec3f &/*p*/) const override final
    {
        return _emission;
    }
};

}


#endif /* DIRECTIONALLIGHT_HPP_ */
