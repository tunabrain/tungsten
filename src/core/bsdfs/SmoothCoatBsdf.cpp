#include "SmoothCoatBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "Fresnel.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

SmoothCoatBsdf::SmoothCoatBsdf()
: _ior(1.3f),
  _thickness(1.0f),
  _sigmaA(0.0f),
  _substrate(std::make_shared<RoughConductorBsdf>())
{
}

void SmoothCoatBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigma_a", _sigmaA);
    _substrate = scene.fetchBsdf(JsonUtils::fetchMember(v, "substrate"));
}

rapidjson::Value SmoothCoatBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "smooth_coat", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("thickness", _thickness, allocator);
    v.AddMember("sigma_a", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    JsonUtils::addObjectMember(v, "substrate", *_substrate, allocator);
    return std::move(v);
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

    if (sampleR && event.sampler->nextBoolean(DiscreteBsdfSample, specularProbability)) {
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
