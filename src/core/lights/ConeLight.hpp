#ifndef CONELIGHT_HPP_
#define CONELIGHT_HPP_

#include "Light.hpp"

#include "math/TangentSpace.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class ConeLight : public Light
{
    static constexpr float LargeT = 100000.0f;

    float _radius;
    float _cosTheta;
    float _solidAngle;
    Vec3f _pos;
    Vec3f _emission;

public:
    ConeLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _radius    (JsonUtils::fromJsonMember<float>(v, "angle")*0.5f),
      _cosTheta  (std::cos(_radius)),
      _solidAngle(TWO_PI*(1.0f - _cosTheta)),
      _emission  (JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    ConeLight(float theta, const Vec3f &emission)
    : _radius(theta*0.5f),
      _cosTheta(std::cos(_radius)),
      _solidAngle(TWO_PI*(1.0f - _cosTheta)),
      _emission(emission)
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "cone", allocator);
        v.AddMember("angle", _radius*2.0f, allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _pos = _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();
    }

    virtual void buildProxy() final override
    {
        std::vector<Vertex> verts;
        std::vector<TriangleI> tris;

        float r = std::sin(_radius);
        for (int i = 0; i < 360; ++i) {
            float a = Angle::degToRad(i);
            verts.emplace_back(Vec3f(std::cos(a)*r, 1.0f, std::sin(a)*r));
            tris.emplace_back(360, i, (i + 1) % 360);
            if (i > 1)
                tris.emplace_back(0, i - 1, i);
        }
        verts.emplace_back(Vec3f(0.0f));

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "ConeLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<ConeLight>(*this);
    }

    bool isDelta() const override final
    {
        return false;
    }

    bool intersect(const Vec3f &p, const Vec3f &w, float &t, Vec3f &q) const override final
    {
        if (w.dot(_pos) < _cosTheta)
            return false;

        t = LargeT;
        q = p + w*t;
        return true;
    }

    bool sample(LightSample &sample) const override final
    {
        float phi      = sample.xi.x()*TWO_PI;
        float cosTheta = sample.xi.y()*(1.0f - _cosTheta) + _cosTheta;
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f wLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentSpace frame(_pos);
        sample.w = frame.toGlobal(wLocal);
        sample.q = sample.p + sample.w*LargeT;
        sample.r = LargeT;
        sample.pdf = 1.0f/_solidAngle;
        sample.L = _emission;
        return true;
    }

    Vec3f eval(const Vec3f &/*w*/) const override final
    {
        return _emission;
    }

    float pdf(const Vec3f &/*p*/, const Vec3f &/*n*/, const Vec3f &/*w*/) const override final
    {
        return 1.0f/_solidAngle;
    }

    Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &/*n*/) const override final
    {
        return approximateRadiance(p);
    }

    Vec3f approximateRadiance(const Vec3f &/*p*/) const override final
    {
        return _emission*_solidAngle;
    }
};

}


#endif /* CONELIGHT_HPP_ */
