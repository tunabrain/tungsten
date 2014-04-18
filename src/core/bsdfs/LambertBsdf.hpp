#ifndef LAMBERTBSDF_HPP_
#define LAMBERTBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/ScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class LambertBsdf : public Bsdf
{
    Vec3f _color;

public:
    LambertBsdf(const rapidjson::Value &v, const Scene &/*scene*/)
    : Bsdf(v),
      _color(JsonUtils::fromJson(v["color"], Vec3f(1.0f)))
    {
        _flags = BsdfFlags(BsdfFlags::DiffuseFlag);
    }

    LambertBsdf(const Vec3f &color)
    : _color(color)
    {
        _flags = BsdfFlags(BsdfFlags::DiffuseFlag);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "lambert", allocator);
        if (_color != Vec3f(1.0f))
            v.AddMember("color", JsonUtils::toJsonValue<float, 3>(_color, allocator), allocator);
        return std::move(v);
    }

    bool sample(ScatterEvent &event) const override final
    {
        event.wi  = cosineHemisphere(event.xi.xy());
        event.pdf = cosineHemispherePdf(event.wi);
        event.throughput = eval(event.wo, event.wi);
        event.switchHemisphere = false;
        return true;
    }

    bool sampleFromLight(ScatterEvent &event) const override final
    {
        event.wi  = uniformHemisphere(event.xi.xy());
        event.pdf = uniformHemispherePdf(event.wi);
        event.throughput = eval(event.wo, event.wi);
        event.switchHemisphere = false;
        return true;
    }

    Vec3f eval(const Vec3f &/*wo*/, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return Vec3f(0.0f);
        return _color*INV_PI;
    }

    float pdf(const Vec3f &/*wo*/, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return 0.0f;
        return cosineHemispherePdf(wi);
    }

    float pdfFromLight(const Vec3f &/*wo*/, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return 0.0f;
        return uniformHemispherePdf(wi);
    }
};

}

#endif /* LAMBERTBSDF_HPP_ */
