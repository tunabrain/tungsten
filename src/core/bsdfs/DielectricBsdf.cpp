#include "DielectricBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

DielectricBsdf::DielectricBsdf()
: _ior(1.5f),
  _enableT(true)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::SpecularTransmissionLobe);
}

DielectricBsdf::DielectricBsdf(float ior)
: _ior(ior),
  _enableT(true)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::SpecularTransmissionLobe);
}

void DielectricBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "enable_refraction", _enableT);
}

rapidjson::Value DielectricBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "dielectric", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("enable_refraction", _enableT, allocator);
    return std::move(v);
}

bool DielectricBsdf::sample(SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::SpecularTransmissionLobe) && _enableT;

    float eta = event.wi.z() < 0.0f ? _ior : _invIor;

    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(eta, std::abs(event.wi.z()), cosThetaT);

    float reflectionProbability;
    if (sampleR && sampleT)
        reflectionProbability = F;
    else if (sampleR)
        reflectionProbability = 1.0f;
    else if (sampleT)
        reflectionProbability = 0.0f;
    else
        return false;

    if (event.sampler->nextBoolean(DiscreteBsdfSample, reflectionProbability)) {
        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = reflectionProbability;
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
        event.weight = sampleT ? Vec3f(1.0f) : Vec3f(F);
    } else {
        if (F == 1.0f)
            return false;

        event.wo = Vec3f(-event.wi.x()*eta, -event.wi.y()*eta, -std::copysign(cosThetaT, event.wi.z()));
        event.pdf = 1.0f - reflectionProbability;
        event.sampledLobe = BsdfLobes::SpecularTransmissionLobe;
        event.weight = sampleR ? Vec3f(1.0f) : Vec3f(1.0f - F);
    }
    event.weight *= albedo(event.info);

    return true;
}

Vec3f DielectricBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool evalT = event.requestedLobe.test(BsdfLobes::SpecularTransmissionLobe) && _enableT;

    float eta = event.wi.z() < 0.0f ? _ior : _invIor;
    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(eta, std::abs(event.wi.z()), cosThetaT);

    if (event.wi.z()*event.wo.z() >= 0.0f) {
        if (evalR && checkReflectionConstraint(event.wi, event.wo))
            return F*albedo(event.info);
        else
            return Vec3f(0.0f);
    } else {
        if (evalT && checkRefractionConstraint(event.wi, event.wo, eta, cosThetaT))
            return (1.0f - F)*albedo(event.info);
        else
            return Vec3f(0.0f);
    }
}

float DielectricBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::SpecularTransmissionLobe) && _enableT;

    float eta = event.wi.z() < 0.0f ? _ior : _invIor;
    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(eta, std::abs(event.wi.z()), cosThetaT);

    if (event.wi.z()*event.wo.z() >= 0.0f) {
        if (sampleR && checkReflectionConstraint(event.wi, event.wo))
            return sampleT ? F : 1.0f;
        else
            return 0.0f;
    } else {
        if (sampleT && checkRefractionConstraint(event.wi, event.wo, eta, cosThetaT))
            return sampleR ? 1.0f - F : 1.0f;
        else
            return 0.0f;
    }
}

float DielectricBsdf::eta(const SurfaceScatterEvent &event) const
{
    if (event.wi.z()*event.wo.z() >= 0.0f)
        return 1.0f;
    else
        return event.wi.z() < 0.0f ? _ior : _invIor;
}

void DielectricBsdf::prepareForRender()
{
    if (_enableT)
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::SpecularTransmissionLobe);
    else
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);

    _invIor = 1.0f/_ior;
}

}
