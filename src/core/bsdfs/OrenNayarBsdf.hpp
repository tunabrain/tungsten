#ifndef ORENNAYARBSDF_HPP_
#define ORENNAYARBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SurfaceScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class OrenNayarBsdf : public Bsdf
{
    std::shared_ptr<TextureA> _roughness;

public:
    OrenNayarBsdf()
    : _roughness(std::make_shared<ConstantTextureA>(0.5f))
    {
        _lobes = BsdfLobes(BsdfLobes::DiffuseReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene);
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return false;
        if (event.wi.z() <= 0.0f)
            return false;
        event.wo  = Sample::cosineHemisphere(event.sampler->next2D());
        event.pdf = Sample::cosineHemispherePdf(event.wo);
        event.throughput = base(event.info);
        event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);

        const Vec3f &wi = event.wi;
        const Vec3f &wo = event.wo;

        float thetaR = std::acos(event.wo.z());
        float thetaI = std::acos(event.wi.z());
        float alpha = max(thetaR, thetaI);
        float beta  = min(thetaR, thetaI);
        float sinAlpha = std::sin(alpha);
        float denom = (wi.x()*wi.x() + wi.y()*wi.y())*(wo.x()*wo.x() + wo.y()*wo.y());
        float cosDeltaPhi;
        if (denom == 0.0f)
            cosDeltaPhi = 1.0f;
        else
            cosDeltaPhi = (wi.x()*wo.x() + wi.y()*wo.y())/std::sqrt(denom);

        float sigma = (*_roughness)[event.info->uv];
        float sigmaSq = sigma*sigma;

        float C1 = 1.0f - 0.5f*sigmaSq/(sigmaSq + 0.33f);
        float C2 = 0.45f*sigmaSq/(sigmaSq + 0.09f);
        if (cosDeltaPhi >= 0.0f)
            C2 *= sinAlpha;
        else
            C2 *= sinAlpha - cube((2.0f*INV_PI)*beta);
        float C3 = 0.125f*(sigmaSq/(sigmaSq + 0.09f))*sqr((4.0f*INV_PI*INV_PI)*alpha*beta);

        float fr1 = INV_PI*(C1 + cosDeltaPhi*C2*std::tan(beta) + (1.0f - std::abs(cosDeltaPhi))*C3*std::tan(0.5f*(alpha + beta)));
        float fr2 = 0.17f*INV_PI*sigmaSq/(sigmaSq + 0.13f)*(1.0f - cosDeltaPhi*sqr((2.0f*INV_PI)*beta));

        Vec3f albedo = base(event.info);
        return albedo*fr1 + albedo*albedo*fr2;
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;
        return Sample::cosineHemispherePdf(event.wo);
    }
};

}

#endif /* ORENNAYARBSDF_HPP_ */
