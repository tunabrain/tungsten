#include "RoughDielectricBsdf.hpp"
#include "Fresnel.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

// Signum that exludes 0
template <typename T>
static inline T sgnE(T val) {
    return val < T(0) ? T(-1) : T(1);
}

RoughDielectricBsdf::RoughDielectricBsdf()
: _distribution("ggx"),
  _roughness(std::make_shared<ConstantTexture>(0.1f)),
  _ior(1.5f),
  _enableT(true)
{
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
}

void RoughDielectricBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("ior", _ior);
    _distribution = value["distribution"];
    value.getField("enable_refraction", _enableT);

    if (auto roughness = value["roughness"])
        _roughness = scene.fetchTexture(roughness, TexelConversion::REQUEST_AVERAGE);

    if (_enableT)
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
    else
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe);
}

rapidjson::Value RoughDielectricBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "rough_dielectric",
        "ior", _ior,
        "distribution", _distribution.toString(),
        "enable_refraction", _enableT,
        "roughness", *_roughness
    };
}

bool RoughDielectricBsdf::sampleBase(SurfaceScatterEvent &event, bool sampleR, bool sampleT,
        float roughness, float ior, Microfacet::Distribution distribution)
{
    float wiDotN = event.wi.z();

    float eta = wiDotN < 0.0f ? ior : 1.0f/ior;

    float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*roughness;
    float alpha = Microfacet::roughnessToAlpha(distribution, roughness);
    float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

    Vec3f m = Microfacet::sample(distribution, sampleAlpha, event.sampler->next2D());
    float pm = Microfacet::pdf(distribution, sampleAlpha, m);

    if (pm < 1e-10f)
        return false;

    float wiDotM = event.wi.dot(m);
    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(1.0f/ior, wiDotM, cosThetaT);
    float etaM = wiDotM < 0.0f ? ior : 1.0f/ior;

    bool reflect;
    if (sampleR && sampleT) {
        reflect = event.sampler->nextBoolean(F);
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

bool RoughDielectricBsdf::invertBase(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event,
        bool sampleR, bool sampleT, float roughness, float ior, Microfacet::Distribution distribution)
{
    float wiDotN = event.wi.z();
    float woDotN = event.wo.z();

    bool reflect = wiDotN*woDotN >= 0.0f;
    if ((reflect && !sampleR) || (!reflect && !sampleT))
        return false;

    float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*roughness;
    float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

    float eta = wiDotN < 0.0f ? ior : 1.0f/ior;
    Vec3d m;
    if (reflect)
        m = Vec3d(event.wi + event.wo).normalized();
    else
        m = Vec3d(event.wi*eta + event.wo).normalized();
    if (m.z() < 0.0f)
        m = -m;
    float wiDotM = event.wi.dot(Vec3f(m));
    float F = Fresnel::dielectricReflectance(1.0f/ior, wiDotM);

    sampler.put2D(Microfacet::invert(distribution, sampleAlpha, m, sampler.untracked1D()));

    if (sampleR && sampleT)
         sampler.putBoolean(F, reflect);

    return true;
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

bool RoughDielectricBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
    float roughness = (*_roughness)[*event.info].x();

    return invertBase(sampler, event, sampleR, sampleT, roughness, _ior, _distribution);
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
    _invIor = 1.0f/_ior;
}

}


