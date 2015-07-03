#include "PlasticBsdf.hpp"
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

PlasticBsdf::PlasticBsdf()
: _ior(1.5f),
  _thickness(1.0f),
  _sigmaA(0.0f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
}

void PlasticBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigma_a", _sigmaA);
}

rapidjson::Value PlasticBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "plastic", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("thickness", _thickness, allocator);
    v.AddMember("sigma_a", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    return std::move(v);
}

bool PlasticBsdf::sample(SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f)
        return false;

    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    const Vec3f &wi = event.wi;
    float eta = 1.0f/_ior;
    float Fi = Fresnel::dielectricReflectance(eta, wi.z());
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
        Vec3f wo(SampleWarp::cosineHemisphere(event.sampler->next2D(BsdfSample)));
        float Fo = Fresnel::dielectricReflectance(eta, wo.z());
        Vec3f diffuseAlbedo = albedo(event.info);

        event.wo = wo;
        event.weight = ((1.0f - Fi)*(1.0f - Fo)*eta*eta)*(diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));
        if (_scaledSigmaA.max() > 0.0f)
            event.weight *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));

        event.pdf = SampleWarp::cosineHemispherePdf(event.wo)*(1.0f - specularProbability);
        event.weight /= 1.0f - specularProbability;
        event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
    }
    return true;
}

Vec3f PlasticBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool evalT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    float eta = 1.0f/_ior;
    float Fi = Fresnel::dielectricReflectance(eta, event.wi.z());
    float Fo = Fresnel::dielectricReflectance(eta, event.wo.z());

    if (evalR && checkReflectionConstraint(event.wi, event.wo)) {
        return Vec3f(Fi);
    } else if (evalT) {
        Vec3f diffuseAlbedo = albedo(event.info);

        Vec3f brdf = ((1.0f - Fi)*(1.0f - Fo)*eta*eta*event.wo.z()*INV_PI)*
                (diffuseAlbedo/(1.0f - diffuseAlbedo*_diffuseFresnel));

        if (_scaledSigmaA.max() > 0.0f)
            brdf *= std::exp(_scaledSigmaA*(-1.0f/event.wo.z() - 1.0f/event.wi.z()));
        return brdf;
    } else {
        return Vec3f(0.0f);
    }
}

float PlasticBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    if (sampleR && sampleT) {
        float Fi = Fresnel::dielectricReflectance(1.0f/_ior, event.wi.z());
        float substrateWeight = _avgTransmittance*(1.0f - Fi);
        float specularWeight = Fi;
        float specularProbability = specularWeight/(specularWeight + substrateWeight);
        if (checkReflectionConstraint(event.wi, event.wo))
            return specularProbability;
        else
            return SampleWarp::cosineHemispherePdf(event.wo)*(1.0f - specularProbability);
    } else if (sampleT) {
        return SampleWarp::cosineHemispherePdf(event.wo);
    } else if (sampleR) {
        return checkReflectionConstraint(event.wi, event.wo) ? 1.0f : 0.0f;
    } else {
        return 0.0f;
    }
}

void PlasticBsdf::prepareForRender()
{
    _scaledSigmaA = _thickness*_sigmaA;
    _avgTransmittance = std::exp(-2.0f*_scaledSigmaA.avg());

    _diffuseFresnel = Fresnel::computeDiffuseFresnel(_ior, 1000000);
}

}
