#include "RoughCoatBsdf.hpp"
#include "RoughDielectricBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "Fresnel.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

RoughCoatBsdf::RoughCoatBsdf()
: _ior(1.3f),
  _thickness(1.0f),
  _sigmaA(0.0f),
  _substrate(std::make_shared<RoughConductorBsdf>()),
  _distributionName("ggx"),
  _roughness(std::make_shared<ConstantTexture>(0.02f))
{
}

void RoughCoatBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigma_a", _sigmaA);
    JsonUtils::fromJson(v, "distribution", _distributionName);
    scene.textureFromJsonMember(v, "roughness", TexelConversion::REQUEST_AVERAGE, _roughness);
    _substrate = scene.fetchBsdf(JsonUtils::fetchMember(v, "substrate"));

    // Fail early in case of invalid distribution name
    prepareForRender();
}

rapidjson::Value RoughCoatBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_coat", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("thickness", _thickness, allocator);
    v.AddMember("sigma_a", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    JsonUtils::addObjectMember(v, "substrate", *_substrate, allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
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

    if (sampleR && (event.sampler->nextBoolean(DiscreteBsdfSample, specularProbability) || !sampleT)) {
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
    _distribution = Microfacet::stringToType(_distributionName);
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe, _substrate->lobes());
}

}
