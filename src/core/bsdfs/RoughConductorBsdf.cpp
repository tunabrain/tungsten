#include "RoughConductorBsdf.hpp"
#include "Microfacet.hpp"
#include "ComplexIor.hpp"
#include "Fresnel.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

RoughConductorBsdf::RoughConductorBsdf()
: _distributionName("ggx"),
  _materialName("Cu"),
  _roughness(std::make_shared<ConstantTexture>(0.1f)),
  _eta(0.200438f, 0.924033f, 1.10221f),
  _k(3.91295f, 2.45285f, 2.14219f)
{
    _lobes = BsdfLobes::GlossyReflectionLobe;
}

void RoughConductorBsdf::lookupMaterial()
{
    if (!ComplexIorList::lookup(_materialName, _eta, _k)) {
        DBG("Warning: Unable to find material with name '%s'. Using default", _materialName.c_str());
        ComplexIorList::lookup("Cu", _eta, _k);
    }
}

void RoughConductorBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    if (JsonUtils::fromJson(v, "eta", _eta) && JsonUtils::fromJson(v, "k", _k))
        _materialName.clear();
    JsonUtils::fromJson(v, "distribution", _distributionName);
    if (JsonUtils::fromJson(v, "material", _materialName))
        lookupMaterial();

    scene.textureFromJsonMember(v, "roughness", TexelConversion::REQUEST_AVERAGE, _roughness);

    // Fail early in case of invalid distribution name
    prepareForRender();
}

rapidjson::Value RoughConductorBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_conductor", allocator);
    if (_materialName.empty()) {
        v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
        v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
    } else {
        v.AddMember("material", _materialName.c_str(), allocator);
    }
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

bool RoughConductorBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f)
        return false;

    // TODO Re-enable this?
    //float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(event.wi.z())))*_roughness;
    float roughness = (*_roughness)[*event.info].x();
    float sampleRoughness = roughness;
    float alpha = Microfacet::roughnessToAlpha(_distribution, roughness);
    float sampleAlpha = Microfacet::roughnessToAlpha(_distribution, sampleRoughness);

    Vec3f m = Microfacet::sample(_distribution, sampleAlpha, event.sampler->next2D(BsdfSample));
    float wiDotM = event.wi.dot(m);
    event.wo = 2.0f*wiDotM*m - event.wi;
    if (wiDotM <= 0.0f || event.wo.z() <= 0.0f)
        return false;
    float G = Microfacet::G(_distribution, alpha, event.wi, event.wo, m);
    float D = Microfacet::D(_distribution, alpha, m);
    float mPdf = Microfacet::pdf(_distribution, sampleAlpha, m);
    float pdf = mPdf*0.25f/wiDotM;
    float weight = wiDotM*G*D/(event.wi.z()*mPdf);
    Vec3f F = Fresnel::conductorReflectance(_eta, _k, wiDotM);

    event.pdf = pdf;
    event.weight = albedo(event.info)*(F*weight);
    event.sampledLobe = BsdfLobes::GlossyReflectionLobe;
    return true;
}

Vec3f RoughConductorBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    float roughness = (*_roughness)[*event.info].x();
    float alpha = Microfacet::roughnessToAlpha(_distribution, roughness);

    Vec3f hr = (event.wi + event.wo).normalized();
    float cosThetaM = event.wi.dot(hr);
    Vec3f F = Fresnel::conductorReflectance(_eta, _k, cosThetaM);
    float G = Microfacet::G(_distribution, alpha, event.wi, event.wo, hr);
    float D = Microfacet::D(_distribution, alpha, hr);
    float fr = (G*D*0.25f)/event.wi.z();

    return albedo(event.info)*(F*fr);
}

float RoughConductorBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    // TODO Re-enable this?
    //float sampleRoughness = (1.2f - 0.2f*std::sqrt(event.wi.z()))*_roughness;
    float roughness = (*_roughness)[*event.info].x();
    float sampleRoughness = roughness;
    float sampleAlpha = Microfacet::roughnessToAlpha(_distribution, sampleRoughness);

    Vec3f hr = (event.wi + event.wo).normalized();
    return Microfacet::pdf(_distribution, sampleAlpha, hr)*0.25f/event.wi.dot(hr);
}

void RoughConductorBsdf::prepareForRender()
{
    _distribution = Microfacet::stringToType(_distributionName);
}

}


