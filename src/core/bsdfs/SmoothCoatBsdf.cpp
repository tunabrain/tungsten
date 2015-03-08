#include "SmoothCoatBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "Fresnel.hpp"

#include "sampling/UniformSampler.hpp"

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
    float specularProbability = specularWeight/(specularWeight + substrateWeight);

    if (sampleR && (event.sampler->next1D() < specularProbability || !sampleT)) {
        event.wo = Vec3f(-wi.x(), -wi.y(), wi.z());
        event.pdf = 0.0f;
        event.throughput = Vec3f(Fi/specularProbability);
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
        event.throughput *= (1.0f - Fi)*(1.0f - Fo);
        if (_scaledSigmaA.max() > 0.0f)
            event.throughput *= std::exp(_scaledSigmaA*(-1.0f/cosThetaSubstrate - 1.0f/cosThetaTi));

        if (sampleR) {
            event.pdf *= 1.0f - specularProbability;
            event.throughput /= 1.0f - specularProbability;
        }

        event.throughput *= originalWi.z()/wiSubstrate.z();
        event.pdf *= eta*eta*cosThetaTo/cosThetaSubstrate;
    }

    return true;
}

Vec3f SmoothCoatBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleT)
        return Vec3f(0.0f);

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    if (Fi == 1.0f || Fo == 1.0f)
        return Vec3f(0.0f);

    Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
    Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

    float laplacian = eta*eta*wi.z()*wo.z()/(cosThetaTi*cosThetaTo);

    Vec3f substrateF = _substrate->eval(event.makeWarpedQuery(wiSubstrate, woSubstrate));

    if (_scaledSigmaA.max() > 0.0f)
        substrateF *= std::exp(_scaledSigmaA*(-1.0f/cosThetaTo - 1.0f/cosThetaTi));

    return laplacian*(1.0f - Fi)*(1.0f - Fo)*substrateF;
}

float SmoothCoatBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(_substrate->lobes());

    if (!sampleT)
        return 0.0f;

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;
    float eta = 1.0f/_ior;

    float cosThetaTi, cosThetaTo;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z(), cosThetaTi);
    float Fo = Fresnel::dielectricReflectance(eta, wo.z(), cosThetaTo);

    if (Fi == 1.0f || Fo == 1.0f)
        return 0.0f;

    Vec3f wiSubstrate(wi.x()*eta, wi.y()*eta, std::copysign(cosThetaTi, wi.z()));
    Vec3f woSubstrate(wo.x()*eta, wo.y()*eta, std::copysign(cosThetaTo, wo.z()));

    float pdf = _substrate->pdf(event.makeWarpedQuery(wiSubstrate, woSubstrate));
    if (sampleR) {
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);
        pdf *= (1.0f - specularProbability);
    }
    pdf *= eta*eta*std::abs(wo.z()/cosThetaTo);
    return pdf;
}

void SmoothCoatBsdf::prepareForRender()
{
    _scaledSigmaA = _thickness*_sigmaA;
    _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe, _substrate->lobes());
}

}
