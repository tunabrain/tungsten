#ifndef DIELECTRICBSDF_HPP_
#define DIELECTRICBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/ScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class DielectricBsdf : public Bsdf
{
    float _ior;
    Vec3f _color;
    uint16 _space;

    float fresnelReflectance(float ior, float cosThetaI, float &cosThetaT) const
    {
        float sinThetaI = std::sqrt(max(1.0f - cosThetaI*cosThetaI, 0.0f));
        float sinThetaT = ior*sinThetaI;
        if (sinThetaT > 1.0f)
            return 1.0f;
        cosThetaT = std::sqrt(max(1.0f - sinThetaT*sinThetaT, 0.0f));

        float Rs = (ior*cosThetaI - cosThetaT)/(ior*cosThetaI + cosThetaT);
        float Rp = (ior*cosThetaT - cosThetaI)/(ior*cosThetaT + cosThetaI);

        return (Rs*Rs + Rp*Rp)*0.5f;
    }

public:
    DielectricBsdf(const rapidjson::Value &v, const Scene &/*scene*/)
    : Bsdf(v),
      _ior  (JsonUtils::fromJsonMember<float>(v, "ior")),
      _color(JsonUtils::fromJson(v["color"], Vec3f(1.0f))),
      _space(JsonUtils::fromJsonMember<float>(v, "space"))
    {
        _flags = BsdfFlags(BsdfFlags::SpecularFlag | BsdfFlags::TransmissiveFlag);
    }

    DielectricBsdf(float ior, const Vec3f &color, uint16 space)
    : _ior(ior), _color(color), _space(space)
    {
        _flags = BsdfFlags(BsdfFlags::SpecularFlag | BsdfFlags::TransmissiveFlag);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "dielectric", allocator);
        v.AddMember("ior", _ior, allocator);
        v.AddMember("space", _space, allocator);
        if (_color != Vec3f(1.0f))
            v.AddMember("color", JsonUtils::toJsonValue<float, 3>(_color, allocator), allocator);
        return std::move(v);
    }

    bool sample(ScatterEvent &event) const override final
    {
        float ior = event.space == _space ? 1.0f/_ior : _ior;
        float cosThetaT = 0.0f;
        if (event.xi.x() <= fresnelReflectance(ior, event.wo.z(), cosThetaT)) {
            event.switchHemisphere = false;
            event.wi = Vec3f(-event.wo.x(), -event.wo.y(), event.wo.z());
        } else {
            event.switchHemisphere = true;
            event.wi = Vec3f(-event.wo.x()*ior, -event.wo.y()*ior, -cosThetaT);
        }
        event.pdf = 1.0f;
        event.throughput = _color/std::abs(event.wi.z());
        return true;
    }

    bool sampleFromLight(ScatterEvent &event) const override final
    {
        return sample(event);
    }

    Vec3f eval(const Vec3f &/*wo*/, const Vec3f &/*wi*/) const override final
    {
        return Vec3f(0.0f);
    }

    float pdf(const Vec3f &/*wo*/, const Vec3f &/*wi*/) const override final
    {
        return 0.0f;
    }

    float pdfFromLight(const Vec3f &/*wo*/, const Vec3f &/*wi*/) const override final
    {
        return 0.0f;
    }
};

}


#endif /* DIELECTRICBSDF_HPP_ */
