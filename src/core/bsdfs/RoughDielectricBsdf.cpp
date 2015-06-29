#include "RoughDielectricBsdf.hpp"
#include "Fresnel.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

// Signum that exludes 0
template <typename T>
static inline T sgnE(T val) {
    return val < T(0) ? T(-1) : T(1);
}

RoughDielectricBsdf::RoughDielectricBsdf()
: _distributionName("ggx"),
  _roughness(std::make_shared<ConstantTexture>(0.1f)),
  _ior(1.5f),
  _enableT(true)
{
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
}

void RoughDielectricBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "distribution", _distributionName);
    JsonUtils::fromJson(v, "enable_refraction", _enableT);

    scene.textureFromJsonMember(v, "roughness", TexelConversion::REQUEST_AVERAGE, _roughness);

    if (_enableT)
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
    else
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe);

    // Fail early in case of invalid distribution name
    prepareForRender();
}

rapidjson::Value RoughDielectricBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_dielectric", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    v.AddMember("enable_refraction", _enableT, allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

bool RoughDielectricBsdf::sampleBase(SurfaceScatterEvent &event, bool sampleR, bool sampleT,
        float roughness, float ior, Microfacet::Distribution distribution)
{
    float wiDotN = event.wi.z();

    float eta = wiDotN < 0.0f ? ior : 1.0f/ior;

    float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*roughness;
    float alpha = Microfacet::roughnessToAlpha(distribution, roughness);
    float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

    Vec3f m = Microfacet::sample(distribution, sampleAlpha, event.sampler->next2D(BsdfSample));
    float pm = Microfacet::pdf(distribution, sampleAlpha, m);

    if (pm < 1e-10f)
        return false;

    float wiDotM = event.wi.dot(m);
    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(1.0f/ior, wiDotM, cosThetaT);
    float etaM = wiDotM < 0.0f ? ior : 1.0f/ior;

    bool reflect;
    if (sampleR && sampleT) {
        reflect = event.sampler->nextBoolean(DiscreteBsdfSample, F);
    } else if (sampleT) {
        if (F == 1.0f)
            return false;
        reflect = false;
    } else if (sampleR) {
        reflect = true;
    } else {
        return false;
    }

    if (reflect)
        event.wo = 2.0f*wiDotM*m - event.wi;
        // Version from the paper (wrong!)
        //event.wo = 2.0f*std::abs(wiDotM)*m - event.wi;
    else
        event.wo = (etaM*wiDotM - sgnE(wiDotM)*cosThetaT)*m - etaM*event.wi;
        // Version from the paper (wrong!)
        //event.wo = (etaM*wiDotM - sgnE(wiDotN)*std::sqrt(1.0f + etaM*(wiDotM*wiDotM - 1.0f)))*m - etaM*event.wi;

    float woDotN = event.wo.z();

    bool reflected = wiDotN*woDotN > 0.0f;
    if (reflected != reflect)
        return false;

    float woDotM = event.wo.dot(m);
    float G = Microfacet::G(distribution, alpha, event.wi, event.wo, m);
    float D = Microfacet::D(distribution, alpha, m);
    event.weight = Vec3f(std::abs(wiDotM)*G*D/(std::abs(wiDotN)*pm));

    if (reflect) {
        event.pdf = pm*0.25f/std::abs(wiDotM);
        event.sampledLobe = BsdfLobes::GlossyReflectionLobe;
    } else {
        event.pdf = pm*std::abs(woDotM)/sqr(eta*wiDotM + woDotM);
        event.sampledLobe = BsdfLobes::GlossyTransmissionLobe;
    }

    if (sampleR && sampleT) {
        if (reflect)
            event.pdf *= F;
        else
            event.pdf *= 1.0f - F;
    } else {
        if (reflect)
            event.weight *= F;
        else
            event.weight *= 1.0f - F;
    }

    return true;
}

Vec3f RoughDielectricBsdf::evalBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
        float roughness, float ior, Microfacet::Distribution distribution)
{
    float wiDotN = event.wi.z();
    float woDotN = event.wo.z();

    bool reflect = wiDotN*woDotN >= 0.0f;
    if ((reflect && !sampleR) || (!reflect && !sampleT))
        return Vec3f(0.0f);

    float alpha = Microfacet::roughnessToAlpha(distribution, roughness);

    float eta = wiDotN < 0.0f ? ior : 1.0f/ior;
    Vec3f m;
    if (reflect)
        m = sgnE(wiDotN)*(event.wi + event.wo).normalized();
    else
        m = -(event.wi*eta + event.wo).normalized();
    float wiDotM = event.wi.dot(m);
    float woDotM = event.wo.dot(m);
    float F = Fresnel::dielectricReflectance(1.0f/ior, wiDotM);
    float G = Microfacet::G(distribution, alpha, event.wi, event.wo, m);
    float D = Microfacet::D(distribution, alpha, m);

    if (reflect) {
        float fr = (F*G*D*0.25f)/std::abs(wiDotN);
        return Vec3f(fr);
    } else {
        float fs = std::abs(wiDotM*woDotM)*(1.0f - F)*G*D/(sqr(eta*wiDotM + woDotM)*std::abs(wiDotN));
        return Vec3f(fs);
    }
}

float RoughDielectricBsdf::pdfBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
        float roughness, float ior, Microfacet::Distribution distribution)
{
    float wiDotN = event.wi.z();
    float woDotN = event.wo.z();

    bool reflect = wiDotN*woDotN >= 0.0f;
    if ((reflect && !sampleR) || (!reflect && !sampleT))
        return 0.0f;

    float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*roughness;
    float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

    float eta = wiDotN < 0.0f ? ior : 1.0f/ior;
    Vec3f m;
    if (reflect)
        m = sgnE(wiDotN)*(event.wi + event.wo).normalized();
    else
        m = -(event.wi*eta + event.wo).normalized();
    float wiDotM = event.wi.dot(m);
    float woDotM = event.wo.dot(m);
    float F = Fresnel::dielectricReflectance(1.0f/ior, wiDotM);
    float pm = Microfacet::pdf(distribution, sampleAlpha, m);

    float pdf;
    if (reflect)
        pdf = pm*0.25f/std::abs(wiDotM);
    else
        pdf = pm*std::abs(woDotM)/sqr(eta*wiDotM + woDotM);
    if (sampleR && sampleT) {
        if (reflect)
            pdf *= F;
        else
            pdf *= 1.0f - F;
    }
    return pdf;
}

bool RoughDielectricBsdf::sample(SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
    float roughness = (*_roughness)[*event.info].x();

    bool result = sampleBase(event, sampleR, sampleT, roughness, _ior, _distribution);
    event.weight *= albedo(event.info);
    return result;
}

Vec3f RoughDielectricBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
    float roughness = (*_roughness)[*event.info].x();

    return evalBase(event, sampleR, sampleT, roughness, _ior, _distribution)*albedo(event.info);
}

float RoughDielectricBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
    float roughness = (*_roughness)[*event.info].x();

    return pdfBase(event, sampleR, sampleT, roughness, _ior, _distribution);
}

float RoughDielectricBsdf::eta(const SurfaceScatterEvent &event) const
{
    if (event.wi.z()*event.wo.z() >= 0.0f)
        return 1.0f;
    else
        return event.wi.z() < 0.0f ? _ior : _invIor;
}

void RoughDielectricBsdf::prepareForRender()
{
    _distribution = Microfacet::stringToType(_distributionName);
    _invIor = 1.0f/_ior;
}

}


