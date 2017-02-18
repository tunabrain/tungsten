#include "SmoothCoatBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "Fresnel.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

SmoothCoatBsdf::SmoothCoatBsdf()
: _ior(1.3f),
  _thickness(1.0f),
  _sigmaA(0.0f),
  _substrate(std::make_shared<RoughConductorBsdf>())
{
}

void SmoothCoatBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("ior", _ior);
    value.getField("thickness", _thickness);
    value.getField("sigma_a", _sigmaA);
    if (auto substrate = value["substrate"])
        _substrate = scene.fetchBsdf(substrate);
}

rapidjson::Value SmoothCoatBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "smooth_coat",
        "ior", _ior,
        "thickness", _thickness,
        "sigma_a", _sigmaA,
        "substrate", *_substrate
    };
}

bool SmoothCoatBsdf::sample(SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f)
        return false;

    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleR && !sampleT)
        return false;

    const Vec3f &wi = event.wi;
    float eta = 1.0f/_ior;

    float cosThetaTi;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);

    float substrateWeight = _avgTransmittance*(1.0f - Fi);
    float specularWeight = Fi;
    float specularProbability;
    if (sampleR && sampleT)
        specularProbability = specularWeight/(specularWeight + substrateWeight);
    else if (sampleR)
        specularProbability = 1.0f;
    else if (sampleT)
        specularProbability = 0.0f;
    else
        return false;

    if (sampleR && event.sampler->nextBoolean(specularProbability)) {
        event.wo = Vec3f(-wi.x(), -wi.y(), wi.z());
        event.pdf = specularProbability;
        event.weight = Vec3f(Fi/specularProbability);
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
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

        event.weight /= 1.0f - specularProbability;
        event.pdf *= 1.0f - specularProbability;
        event.pdf *= eta*eta*cosThetaTo/cosThetaSubstrate;
    }

    return true;
}

bool SmoothCoatBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f)
        return false;

    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleR && !sampleT)
        return false;

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);

    float substrateWeight = _avgTransmittance*(1.0f - Fi);
    float specularWeight = Fi;
    float specularProbability;
    if (sampleR && sampleT)
        specularProbability = specularWeight/(specularWeight + substrateWeight);
    else if (sampleR)
        specularProbability = 1.0f;
    else if (sampleT)
        specularProbability = 0.0f;
    else
        return false;

    if (sampleR && checkReflectionConstraint(event.wi, event.wo)) {
        sampler.putBoolean(specularProbability, true);
        return true;
    } else if (sampleT) {
        if (sampleR)
            sampler.putBoolean(specularProbability, false);

        Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
        Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));
        return _substrate->invert(sampler, event.makeWarpedQuery(wiSubstrate, woSubstrate));
    }
    return false;
}

Vec3f SmoothCoatBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool evalT = event.requestedLobe.test(_substrate->lobes());

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    if (evalR && checkReflectionConstraint(event.wi, event.wo)) {
        return Vec3f(Fi);
    } else if (evalT) {
        Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
        Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

        float laplacian = eta*eta*wo.z()/cosThetaTo;

        Vec3f substrateF = _substrate->eval(event.makeWarpedQuery(wiSubstrate, woSubstrate));

        if (_scaledSigmaA.max() > 0.0f)
            substrateF *= std::exp(_scaledSigmaA*(-1.0f/cosThetaTo - 1.0f/cosThetaTi));

        return laplacian*(1.0f - Fi)*(1.0f - Fo)*substrateF;
    } else {
        return Vec3f(0.0f);
    }
}

float SmoothCoatBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
    Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

    if (sampleR && sampleT) {
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);
        if (checkReflectionConstraint(event.wi, event.wo))
            return specularProbability;
        else
            return _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate))
                    *(1.0f - specularProbability)*eta*eta*std::abs(wo.z()/cosThetaTo);
    } else if (sampleT) {
        return _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate))*eta*eta*std::abs(wo.z()/cosThetaTo);
    } else if (sampleR) {
        return checkReflectionConstraint(event.wi, event.wo) ? 1.0f : 0.0f;
    } else {
        return 0.0f;
    }
}

void SmoothCoatBsdf::prepareForRender()
{
    _scaledSigmaA = _thickness*_sigmaA;
    _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe, _substrate->lobes());
}

}
