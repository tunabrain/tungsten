#ifndef ROUGHPLASTICBSDF_HPP_
#define ROUGHPLASTICBSDF_HPP_

#include <rapidjson/document.h>

#include "RoughDielectricBsdf.hpp"
#include "Fresnel.hpp"
#include "Bsdf.hpp"

#include "sampling/SurfaceScatterEvent.hpp"
#include "sampling/SampleGenerator.hpp"
#include "sampling/Sample.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class RoughPlasticBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;
    std::string _distributionName;
    std::shared_ptr<TextureA> _roughness;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;
    Microfacet::Distribution _distribution;

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
    RoughPlasticBsdf()
    : _ior(1.5f),
      _thickness(0.0f),
      _sigmaA(0.0f),
      _distributionName("ggx"),
      _roughness(std::make_shared<ConstantTextureA>(0.02f))
    {
        init();
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
    }

    RoughPlasticBsdf(float ior)
    : _ior(ior),
      _thickness(0.0f),
      _sigmaA(0.0f),
      _distributionName("ggx"),
      _roughness(std::make_shared<ConstantTextureA>(0.02f))
    {
        init();
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (event.wi.z() <= 0.0f)
            return false;

        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

        if (!sampleR && !sampleT)
            return false;

        const Vec3f &wi = event.wi;
        float eta = 1.0f/_ior;
        float Fi = Fresnel::dielectricReflectance(eta, wi.z());
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);

        if (sampleR && (event.sampler->next1D() < specularProbability || !sampleT)) {
            float roughness = (*_roughness)[event.info->uv];
            if (!RoughDielectricBsdf::sampleBase(event, true, false, roughness, _ior, _distribution))
                return false;
            if (sampleT) {
                Vec3f albedo = base(event.info);
                float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

                Vec3f brdfSubstrate = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(albedo/(1.0f - albedo*_diffuseFresnel))*INV_PI*event.wo.z();
                Vec3f brdfSpecular = event.throughput*event.pdf;
                float pdfSubstrate = Sample::cosineHemispherePdf(event.wo)*(1.0f - specularProbability);
                float pdfSpecular = event.pdf*specularProbability;

                event.throughput = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
                event.pdf = pdfSpecular + pdfSubstrate;
            }
            return true;
        } else {
            Vec3f wo(Sample::cosineHemisphere(event.sampler->next2D()));
            float Fo = Fresnel::dielectricReflectance(eta, wo.z());
            Vec3f albedo = base(event.info);

            event.wo = wo;
            event.throughput = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(albedo/(1.0f - albedo*_diffuseFresnel));
            if (_scaledSigmaA.max() > 0.0f)
                event.throughput *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));

            event.pdf = Sample::cosineHemispherePdf(event.wo);
            if (sampleR) {
                Vec3f brdfSubstrate = event.throughput*event.pdf;
                float  pdfSubstrate = event.pdf*(1.0f - specularProbability);
                Vec3f brdfSpecular = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[event.info->uv], _ior, _distribution);
                float pdfSpecular  = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[event.info->uv], _ior, _distribution);
                pdfSpecular *= specularProbability;

                event.throughput = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
                event.pdf = pdfSpecular + pdfSubstrate;
            }
            event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
        }
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
        if (!sampleR && !sampleT)
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);

        Vec3f glossyR(0.0f);
        if (sampleR)
            glossyR = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[event.info->uv], _ior, _distribution);

        Vec3f diffuseR(0.0f);
        if (sampleT) {
            float eta = 1.0f/_ior;
            float Fi = Fresnel::dielectricReflectance(eta, event.wi.z());
            float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

            Vec3f albedo = base(event.info);

            diffuseR = ((1.0f - Fi)*(1.0f - Fo)*eta*eta*event.wo.z()*INV_PI)*(albedo/(1.0f - albedo*_diffuseFresnel));
            if (_scaledSigmaA.max() > 0.0f)
                diffuseR *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));
        }

        return glossyR + diffuseR;
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
        if (!sampleR && !sampleT)
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;

        float glossyPdf = 0.0f;
        if (sampleR)
            glossyPdf = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[event.info->uv], _ior, _distribution);

        float diffusePdf = 0.0f;
        if (sampleT)
            diffusePdf = Sample::cosineHemispherePdf(event.wo);

        if (sampleT && sampleR) {
            float Fi = Fresnel::dielectricReflectance(1.0f/_ior, event.wi.z());
            float substrateWeight = _avgTransmittance*(1.0f - Fi);
            float specularWeight = Fi;
            float specularProbability = specularWeight/(specularWeight + substrateWeight);

            diffusePdf *= (1.0f - specularProbability);
            glossyPdf *= specularProbability;
        }
        return glossyPdf + diffusePdf;
    }

    float ior() const
    {
        return _ior;
    }
};

}


#endif /* ROUGHPLASTICBSDF_HPP_ */
