#include "RoughCoatBsdf.hpp"
#include "RoughDielectricBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "Fresnel.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

RoughCoatBsdf::RoughCoatBsdf()
: _ior(1.3f),
  _thickness(1.0f),
  _sigmaA(0.0f),
  _substrate(std::make_shared<RoughConductorBsdf>()),
  _distribution("ggx"),
  _roughness(std::make_shared<ConstantTexture>(0.02f))
{
}

void RoughCoatBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("ior", _ior);
    value.getField("thickness", _thickness);
    value.getField("sigma_a", _sigmaA);
    _distribution = value["distribution"];
    if (auto roughness = value["roughness"]) _roughness = scene.fetchTexture(roughness, TexelConversion::REQUEST_AVERAGE);
    if (auto substrate = value["substrate"]) _substrate = scene.fetchBsdf(substrate);
}

rapidjson::Value RoughCoatBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "rough_coat",
        "ior", _ior,
        "thickness", _thickness,
        "sigma_a", _sigmaA,
        "substrate", *_substrate,
        "distribution", _distribution.toString(),
        "roughness", *_roughness
    };
}

void RoughCoatBsdf::substrateEvalAndPdf(const SurfaceScatterEvent &event, float eta,
        float Fi, float cosThetaTi, float &pdf, Vec3f &brdf) const
{
    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;

    float cosThetaTo;
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    if (Fi == 1.0f || Fo == 1.0f) {
        pdf = 0.0f;
        brdf = Vec3f(0.0f);
        return;
    }

    Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
    Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

    pdf = _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate));
    pdf *= eta*eta*std::abs(wo.z()/cosThetaTo);

    float compressionProjection = eta*eta*wo.z()/cosThetaTo;

    Vec3f substrateF = _substrate->eval(event.makeWarpedQuery(wiSubstrate, woSubstrate));

    if (_scaledSigmaA.max() > 0.0f)
        substrateF *= std::exp(_scaledSigmaA*(-1.0f/cosThetaTo - 1.0f/cosThetaTi));

    brdf = compressionProjection*(1.0f - Fi)*(1.0f - Fo)*substrateF;
}

bool RoughCoatBsdf::sample(SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f)
        return false;

    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleR && !sampleT)
        return false;

    const Vec3f &wi = event.wi;
    float eta = 1.0f/_ior;

    float cosThetaTi;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);

    float substrateWeight = _avgTransmittance*(1.0f - Fi);
    float specularWeight = Fi;
    float specularProbability = specularWeight/(specularWeight + substrateWeight);

    if (sampleR && (event.sampler->nextBoolean(specularProbability) || !sampleT)) {
        float roughness = (*_roughness)[*event.info].x();
        if (!RoughDielectricBsdf::sampleBase(event, true, false, roughness, _ior, _distribution))
            return false;
        if (sampleT) {
            Vec3f brdfSubstrate, brdfSpecular = event.weight*event.pdf;
            float  pdfSubstrate,  pdfSpecular = event.pdf*specularProbability;
            substrateEvalAndPdf(event, eta, Fi, cosThetaTi, pdfSubstrate, brdfSubstrate);
            pdfSubstrate *= 1.0f - specularProbability;

            event.weight = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
            event.pdf = pdfSpecular + pdfSubstrate;
        }
        return true;
    } else {
        Vec3f originalWi(wi);
        Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, cosThetaTi);
        event.wi = wiSubstrate;
        bool success = _substrate->sample(event);
        event.wi = originalWi;
        if (!success)
            return false;

        float cosThetaTo;
        float Fo = Fresnel::dielectricReflectance(_ior, event.wo.z(), cosThetaTo);
        if (Fo == 1.0f)
            return false;
        float cosThetaSubstrate = event.wo.z();
        event.wo = Vec3f(event.wo.x()*_ior, event.wo.y()*_ior, cosThetaTo);
        event.weight *= (1.0f - Fi)*(1.0f - Fo);
        if (_scaledSigmaA.max() > 0.0f)
            event.weight *= std::exp(_scaledSigmaA*(-1.0f/cosThetaSubstrate - 1.0f/cosThetaTi));

        event.weight *= originalWi.z()/wiSubstrate.z();
        event.pdf *= eta*eta*cosThetaTo/cosThetaSubstrate;

        if (sampleR) {
            Vec3f brdfSubstrate = event.weight*event.pdf;
            float  pdfSubstrate = event.pdf*(1.0f - specularProbability);
            Vec3f brdfSpecular = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);
            float pdfSpecular  = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);
            pdfSpecular *= specularProbability;

            event.weight = (brdfSpecular + brdfSubstrate)/(pdfSpecular + pdfSubstrate);
            event.pdf = pdfSpecular + pdfSubstrate;
        }
    }

    return true;
}

Vec3f RoughCoatBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleT && !sampleR)
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    Vec3f glossyR(0.0f);
    if (sampleR)
        glossyR = RoughDielectricBsdf::evalBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);


    Vec3f substrateR(0.0f);
    if (sampleT) {
        const Vec3f &wi = event.wi;
        const Vec3f &wo = event.wo;
        float eta = 1.0f/_ior;

        float cosThetaTi, cosThetaTo;
        float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
        float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

        if (Fi == 1.0f || Fo == 1.0f)
            return glossyR;

        Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
        Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

        float compressionProjection = eta*eta*wo.z()/cosThetaTo;

        Vec3f substrateF = _substrate->eval(event.makeWarpedQuery(wiSubstrate, woSubstrate));

        if (_scaledSigmaA.max() > 0.0f)
            substrateF *= std::exp(_scaledSigmaA*(-1.0f/cosThetaTo - 1.0f/cosThetaTi));

        substrateR = compressionProjection*(1.0f - Fi)*(1.0f - Fo)*substrateF;
    }

    return glossyR + substrateR;
}

bool RoughCoatBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleT && !sampleR)
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    float specularProbability;
    if (sampleR && sampleT) {
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        specularProbability = specularWeight/(specularWeight + substrateWeight);
    } else {
        specularProbability = sampleR ? 1.0f : 0.0f;
    }

    float glossyPdf = 0.0f;
    if (sampleR)
        glossyPdf = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);

    float substratePdf = 0.0f;
    Vec3f wiSubstrate, woSubstrate;
    if (sampleT) {
        if (Fi < 1.0f && Fo < 1.0f) {
            wiSubstrate = Vec3f(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
            woSubstrate = Vec3f(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

            substratePdf = _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate));
            substratePdf *= eta*eta*std::abs(wo.z()/cosThetaTo);
        }
    }

    float pdf0 = glossyPdf*specularProbability;
    float pdf1 = substratePdf*(1.0f - specularProbability);
    if (pdf0 == 0.0f && pdf1 == 0.0f)
        return false;

    if (sampler.untrackedBoolean(pdf0/(pdf0 + pdf1))) {
        sampler.putBoolean(specularProbability, true);
        float roughness = (*_roughness)[*event.info].x();
        return RoughDielectricBsdf::invertBase(sampler, event, true, false, roughness, _ior, _distribution);
    } else {
        if (sampleR)
            sampler.putBoolean(specularProbability, false);
        return _substrate->invert(sampler, event.makeWarpedQuery(wiSubstrate, woSubstrate));
    }
}

float RoughCoatBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleT && !sampleR)
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    float specularProbability;
    if (sampleR && sampleT) {
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        specularProbability = specularWeight/(specularWeight + substrateWeight);
    } else {
        specularProbability = sampleR ? 1.0f : 0.0f;
    }

    float glossyPdf = 0.0f;
    if (sampleR)
        glossyPdf = RoughDielectricBsdf::pdfBase(event, true, false, (*_roughness)[*event.info].x(), _ior, _distribution);

    float substratePdf = 0.0f;
    if (sampleT) {
        if (Fi < 1.0f && Fo < 1.0f) {
            Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
            Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

            substratePdf = _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate));
            substratePdf *= eta*eta*std::abs(wo.z()/cosThetaTo);
        }
    }

    return glossyPdf*specularProbability + substratePdf*(1.0f - specularProbability);
}

void RoughCoatBsdf::prepareForRender()
{
    _scaledSigmaA = _thickness*_sigmaA;
    _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe, _substrate->lobes());
}

}
