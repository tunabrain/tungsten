#ifndef PHONGBSDF_HPP_
#define PHONGBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/ScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/TangentFrame.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class PhongBsdf : public Bsdf
{
    int _hardness;
    float _exponent;
    float _invExponent;
    float _pdfFactor;
    float _brdfFactor;

    void init()
    {
        _lobes       = BsdfLobes(BsdfLobes::GlossyReflectionLobe);
        _exponent    = float(_hardness);
        _invExponent = 1.0f/float(1 + _hardness);
        _pdfFactor   = float(_hardness + 1)*INV_TWO_PI;
        _brdfFactor  = float(_hardness + 2)*INV_TWO_PI;
    }

public:
    PhongBsdf()
    : _hardness(64)
    {
        init();
    }

    PhongBsdf(int hardness)
    : _hardness(hardness)
    {
        init();
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "hardness", _hardness);
        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "phong", allocator);
        v.AddMember("hardness", _hardness, allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return false;
        if (event.wi.z() <= 0.0f)
            return false;

        Vec2f xi = event.sampler.next2D();
        float phi      = xi.x()*TWO_PI;
        float cosTheta = std::pow(xi.y(), _invExponent);
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f woLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentFrame lobe(Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()));
        event.wo = lobe.toGlobal(woLocal);
        if (event.wo.z() < 0.0f)
            return false;

        cosTheta = lobe.normal.dot(event.wo);
        float cosThetaPowN = std::pow(cosTheta, _exponent);
        event.pdf = cosThetaPowN*_pdfFactor;
        event.throughput = base(event.info)*(event.wo.z()*(_brdfFactor/_pdfFactor));
//      Vec3f f = eval(event);
//      Vec3f diff = event.throughput*event.pdf - f;
//      if (std::abs(diff.max()) > 1e-5f) {
//          std::cout << diff << " " << name() << std::endl;
//      }
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);
        float cosTheta = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo);
        if (cosTheta < 0.0f)
            return Vec3f(0.0f);
        return base(event.info)*(std::pow(cosTheta, _exponent)*_brdfFactor*event.wo.z());
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;
        return std::pow(Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo), _exponent)*_pdfFactor;
    }
};

}


#endif /* PHONGBSDF_HPP_ */
