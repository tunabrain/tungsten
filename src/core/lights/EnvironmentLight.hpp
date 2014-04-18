#ifndef ENVIRONMENTLIGHT_HPP_
#define ENVIRONMENTLIGHT_HPP_

#include "Light.hpp"

#include "materials/AtmosphericScattering.hpp"

#include "math/TangentSpace.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class EnvironmentLight : public Light
{
    static constexpr float LargeT = 100000.0f;
    static constexpr float DirectRatio = 0.90f;

    std::shared_ptr<AtmosphericScattering> _scatter;

    float _radius;
    float _cosTheta;
    float _solidAngle;

    float _indirectPdf;
    float _directPdf;

    Vec3f _sunDir;
    Vec3f _emission;

public:
    EnvironmentLight(float theta, const Vec3f &emission)
    : _radius(theta*0.5f),
      _cosTheta(std::cos(_radius)),
      _solidAngle(TWO_PI*(1.0f - _cosTheta)),
      _indirectPdf((1.0f - DirectRatio)/(TWO_PI - _solidAngle)),
      _directPdf(DirectRatio/_solidAngle),
      _emission(emission)
    {
    }

    EnvironmentLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _radius    (JsonUtils::fromJsonMember<float>(v, "angle")*0.5f),
      _cosTheta  (std::cos(_radius)),
      _solidAngle(TWO_PI*(1.0f - _cosTheta)),
      _indirectPdf((1.0f - DirectRatio)/(TWO_PI - _solidAngle)),
      _directPdf(DirectRatio/_solidAngle),
      _emission  (JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "environment", allocator);
        v.AddMember("angle", _radius*2.0f, allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _sunDir = _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();

        if (!_scatter) {
            _scatter = std::make_shared<AtmosphericScattering>(AtmosphereParameters::generic());
            _scatter->precompute();
        }
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

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "EnvironmentLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<EnvironmentLight>(*this);
    }

    bool isDelta() const override final
    {
        return false;
    }

    bool intersect(const Vec3f &p, const Vec3f &w, float &t, Vec3f &q) const override final
    {
        t = LargeT;
        q = p + w*t;
        return true;
    }

    bool sample(LightSample &sample) const override final
    {
        float cosTheta;
        if (sample.xi.x() > DirectRatio) {
            sample.xi.x() = clamp((sample.xi.x() - DirectRatio)/(1.0f - DirectRatio), 0.0f, 1.0f);
            cosTheta = sample.xi.y()*(_cosTheta + 1.0f) - 1.0f;
        } else {
            sample.xi.x() /= DirectRatio;
            cosTheta = sample.xi.y()*(1.0f - _cosTheta) + _cosTheta;
        }
        float phi      = sample.xi.x()*TWO_PI;
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f wLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentSpace frame(_sunDir);
        sample.w = frame.toGlobal(wLocal);
        sample.q = sample.p + sample.w*LargeT;
        sample.r = LargeT;
        sample.pdf = 1.0f/_solidAngle;
        sample.L = _emission;
        return true;
    }

    Vec3f eval(const Vec3f &w) const override final
    {
        if (w.dot(_sunDir)*0.95f < _cosTheta)
            return _scatter->evalSimple(w, _sunDir, _emission)*Vec3f(30.0f);
        else
            return _emission;
    }

    float pdf(const Vec3f &/*p*/, const Vec3f &/*n*/, const Vec3f &w) const override final
    {
        if (w.dot(_sunDir) < _cosTheta)
            return _indirectPdf;
        else
            return _directPdf;
    }

    Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &/*n*/) const override final
    {
        return approximateRadiance(p);
    }

    Vec3f approximateRadiance(const Vec3f &/*p*/) const override final
    {
        return _emission*_solidAngle/DirectRatio;
    }
};

}


#endif /* ENVIRONMENTLIGHT_HPP_ */
