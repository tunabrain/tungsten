#ifndef PHONGBSDF_HPP_
#define PHONGBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/ScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/TangentSpace.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class PhongBsdf : public Bsdf
{
    Vec3f _color;
    int _hardness;
    float _exponent;
    float _invExponent;
    float _pdfFactor;
    float _brdfFactor;

    void init()
    {
        _flags       = BsdfFlags(BsdfFlags::GlossyFlag);
        _exponent    = float(_hardness);
        _invExponent = 1.0f/float(1 + _hardness);
        _pdfFactor   = float(_hardness + 1)*INV_TWO_PI;
        _brdfFactor  = float(_hardness + 2)*INV_TWO_PI;
    }

public:
    PhongBsdf(const rapidjson::Value &v, const Scene &/*scene*/)
    : Bsdf(v),
      _color(JsonUtils::fromJson(v["color"], Vec3f(1.0f))),
      _hardness(JsonUtils::fromJsonMember<int>(v, "hardness"))
    {
        init();
    }

    PhongBsdf(const Vec3f &color, int hardness)
    : _color(color),
      _hardness(hardness)
    {
        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "phong", allocator);
        if (_color != Vec3f(1.0f))
            v.AddMember("color", JsonUtils::toJsonValue<float, 3>(_color, allocator), allocator);
        v.AddMember("hardness", _hardness, allocator);
        return std::move(v);
    }

    bool sample(ScatterEvent &event) const override final
    {
        float phi      = event.xi.x()*TWO_PI;
        float cosTheta = std::pow(event.xi.y(), _invExponent);
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f wiLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentSpace lobe(Vec3f(-event.wo.x(), -event.wo.y(), event.wo.z()));
        event.wi = lobe.toGlobal(wiLocal);
        if (event.wi.z() < 0.0f)
            return false;

        float cosThetaPowN = std::pow(cosTheta, _exponent);
        event.pdf = cosThetaPowN*_pdfFactor;
        event.throughput = _color*cosThetaPowN*_brdfFactor;
        event.switchHemisphere = false;
        return true;
    }

    bool sampleFromLight(ScatterEvent &event) const override final
    {
        return sample(event);
    }

    Vec3f eval(const Vec3f &wo, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return Vec3f(0.0f);
        float cosTheta = Vec3f(-wo.x(), -wo.y(), wo.z()).dot(wi);
        if (cosTheta < 0.0f)
            return Vec3f(0.0f);
        return _color*std::pow(cosTheta, _exponent)*_brdfFactor;
    }

    float pdf(const Vec3f &wo, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return 0.0f;
        return std::pow(Vec3f(-wo.x(), -wo.y(), wo.z()).dot(wi), _exponent)*_pdfFactor;
    }

    float pdfFromLight(const Vec3f &wo, const Vec3f &wi) const override final
    {
        if (wi.z() <= 0.0f)
            return 0.0f;
        return pdf(wo, wi);
    }
};

}


#endif /* PHONGBSDF_HPP_ */
