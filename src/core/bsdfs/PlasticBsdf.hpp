#ifndef PLASTICBSDF_HPP_
#define PLASTICBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class PlasticBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;

    void computeDiffuseFresnel()
    {
        constexpr int SampleCount = 10000;

        _diffuseFresnel = 0.0f;
        float fb = Fresnel::dielectricReflectance(_ior, 0.0f);
        for (int i = 1; i <= SampleCount; ++i) {
            float cosThetaSq = float(i)/SampleCount;
            float fa = Fresnel::dielectricReflectance(_ior, min(std::sqrt(cosThetaSq), 1.0f));
            _diffuseFresnel += (fa + fb)*0.5f/SampleCount;
            fb = fa;
        }
    }

    void init()
    {
        _scaledSigmaA = _thickness*_sigmaA;
        _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());

        computeDiffuseFresnel();
    }

public:
    PlasticBsdf()
    : _ior(1.5f),
      _thickness(0.0f),
      _sigmaA(0.0f)
    {
        init();
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
    }

    PlasticBsdf(float ior)
    : _ior(ior),
      _thickness(0.0f),
      _sigmaA(0.0f)
    {
        init();
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "ior", _ior);
        JsonUtils::fromJson(v, "thickness", _thickness);
        JsonUtils::fromJson(v, "sigmaA", _sigmaA);

        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "plastic", allocator);
        v.AddMember("ior", _ior, allocator);
        v.AddMember("thickness", _thickness, allocator);
        v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (event.wi.z() <= 0.0f)
            return false;

        bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

        const Vec3f &wi = event.wi;
        float eta = 1.0f/_ior;
        float Fi = Fresnel::dielectricReflectance(eta, wi.z());
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);

        if (sampleR && (event.sampler->next1D() < specularProbability || !sampleT)) {
            event.wo = Vec3f(-wi.x(), -wi.y(), wi.z());
            if (sampleT) {
                event.pdf = specularProbability;
                event.throughput = Vec3f(Fi/specularProbability);
            } else {
                event.pdf = 1.0f;
                event.throughput = Vec3f(Fi);
            }
            event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
        } else {
            Vec3f wo(Sample::cosineHemisphere(event.sampler->next2D()));
            float Fo = Fresnel::dielectricReflectance(eta, wo.z());
            Vec3f diffuseAlbedo = albedo(event.info);

            event.wo = wo;
            event.throughput = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));
            if (_scaledSigmaA.max() > 0.0f)
                event.throughput *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));

            event.pdf = Sample::cosineHemispherePdf(event.wo);
            if (sampleR) {
                event.pdf *= 1.0f - specularProbability;
                event.throughput /= 1.0f - specularProbability;
            }
            event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
        }
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);

        float eta = 1.0f/_ior;
        float Fi = Fresnel::dielectricReflectance(eta, event.wi.z());
        float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

        Vec3f diffuseAlbedo = albedo(event.info);

        Vec3f brdf = ((1.0f - Fi)*(1.0f - Fo)*eta*eta*event.wo.z()*INV_PI)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));

        if (_scaledSigmaA.max() > 0.0f)
            brdf *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));

        return brdf;
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;

        bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

        if (!sampleT)
            return 0.0f;

        float pdf = Sample::cosineHemispherePdf(event.wo);
        if (sampleR) {
            float Fi = Fresnel::dielectricReflectance(1.0f/_ior, event.wi.z());
            float substrateWeight = _avgTransmittance*(1.0f - Fi);
            float specularWeight = Fi;
            float specularProbability = specularWeight/(specularWeight + substrateWeight);
            pdf *= (1.0f - specularProbability);
        }
        return pdf;
    }

    float ior() const
    {
        return _ior;
    }
};

}


#endif /* DIELECTRICBSDF_HPP_ */
