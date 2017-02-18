#include "RoughPlasticBsdf.hpp"
#include "RoughDielectricBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

RoughPlasticBsdf::RoughPlasticBsdf()
: _ior(1.5f),
  _thickness(1.0f),
  _sigmaA(0.0f),
  _distribution("ggx"),
  _roughness(std::make_shared<ConstantTexture>(0.02f))
{
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
}

void RoughPlasticBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("ior", _ior);
    _distribution = value["distribution"];
    value.getField("thickness", _thickness);
    value.getField("sigma_a", _sigmaA);
    if (auto roughness = value["roughness"])
        _roughness = scene.fetchTexture(roughness, TexelConversion::REQUEST_AVERAGE);
}

rapidjson::Value RoughPlasticBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "rough_plastic",
        "ior", _ior,
        "thickness", _thickness,
        "sigma_a", _sigmaA,
        "distribution", _distribution.toString(),
        "roughness", *_roughness
    };
}

bool RoughPlasticBsdf::sample(SurfaceScatterEvent &event) const
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
    float substrateWeight = _substrateWeight*_avgTransmittance*(1.0f - Fi);
    float specularWeight = Fi;
    float specularProbability = specularWeight/(specularWeight + substrateWeight);

    if (sampleR && (event.sampler->nextBoolean(specularProbability) || !sampleT)) {
        float roughness = (*_roughness)[*event.info].x();
        if (!RoughDielectricBsdf::sampleBase(event, true, false, roughness, _ior, _distribution))
            return false;
        if (sampleT) {
            Vec3f diffuseAlbedo = albedo(event.info);
            float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

            Vec3f brdfSubstrate = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel))*INV_PI*event.wo.z();
            Vec3f brdfSpecular = event.weight*event.pdf;
            float pdfSubstrate = SampleWarp::cosineHemispherePdf(event.wo)*(1.0f - specularProbability);
            float pdfSpecular = event.pdf*specularProbability;

            event.weight = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
            event.pdf = pdfSpecular + pdfSubstrate;
        }
        return true;
    } else {
        Vec3f wo(SampleWarp::cosineHemisphere(event.sampler->next2D()));
        float Fo = Fresnel::dielectricReflectance(eta, wo.z());
        Vec3f diffuseAlbedo = albedo(event.info);

        event.wo = wo;
        event.weight = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));
        if (_scaledSigmaA.max() > 0.0f)
            event.weight *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));

        event.pdf = SampleWarp::cosineHemispherePdf(event.wo);
        if (sampleR) {
            Vec3f brdfSubstrate = event.weight*event.pdf;
            float  pdfSubstrate = event.pdf*(1.0f - specularProbability);
            Vec3f brdfSpecular = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);
            float pdfSpecular  = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);
            pdfSpecular *= specularProbability;

            event.weight = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
            event.pdf = pdfSpecular + pdfSubstrate;
        }
        event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
    }
    return true;
}

Vec3f RoughPlasticBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
    if (!sampleR && !sampleT)
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    Vec3f glossyR(0.0f);
    if (sampleR)
        glossyR = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);

    Vec3f diffuseR(0.0f);
    if (sampleT) {
        float eta = 1.0f/_ior;
        float Fi = Fresnel::dielectricReflectance(eta, event.wi.z());
        float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

        Vec3f diffuseAlbedo = albedo(event.info);

        diffuseR = ((1.0f - Fi)*(1.0f - Fo)*eta*eta*event.wo.z()*INV_PI)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));
        if (_scaledSigmaA.max() > 0.0f)
            diffuseR *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));
    }

    return glossyR + diffuseR;
}

bool RoughPlasticBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return false;

    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    if (!sampleR && !sampleT)
        return false;

    float glossyPdf = 0.0f;
    if (sampleR)
        glossyPdf = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);

    float diffusePdf = 0.0f;
    if (sampleT)
        diffusePdf = SampleWarp::cosineHemispherePdf(event.wo);

    float Fi = Fresnel::dielectricReflectance(1.0f/_ior, event.wi.z());
    float substrateWeight = _substrateWeight*_avgTransmittance*(1.0f - Fi);
    float specularWeight = Fi;
    float specularProbability = specularWeight/(specularWeight + substrateWeight);

    if (sampleT && sampleR) {
        diffusePdf *= (1.0f - specularProbability);
        glossyPdf *= specularProbability;
    }

    if (sampler.untrackedBoolean(glossyPdf/(diffusePdf + glossyPdf))) {
        sampler.putBoolean(specularProbability, true);
        float roughness = (*_roughness)[*event.info].x();
        return RoughDielectricBsdf::invertBase(sampler, event, true, false, roughness, _ior, _distribution);
    } else {
        if (sampleR)
            sampler.putBoolean(specularProbability, false);
        sampler.put2D(SampleWarp::invertCosineHemisphere(event.wo, sampler.untracked1D()));
        return true;
    }
    return false;
}

float RoughPlasticBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
    if (!sampleR && !sampleT)
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    float glossyPdf = 0.0f;
    if (sampleR)
        glossyPdf = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);

    float diffusePdf = 0.0f;
    if (sampleT)
        diffusePdf = SampleWarp::cosineHemispherePdf(event.wo);

    if (sampleT && sampleR) {
        float Fi = Fresnel::dielectricReflectance(1.0f/_ior, event.wi.z());
        float substrateWeight = _substrateWeight*_avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);

        diffusePdf *= (1.0f - specularProbability);
        glossyPdf *= specularProbability;
    }
    return glossyPdf + diffusePdf;
}

void RoughPlasticBsdf::prepareForRender()
{
    _scaledSigmaA = _thickness*_sigmaA;
    _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());
    _substrateWeight = _albedo->average().avg();

    _diffuseFresnel = Fresnel::computeDiffuseFresnel(_ior, 1000000);
}

}
