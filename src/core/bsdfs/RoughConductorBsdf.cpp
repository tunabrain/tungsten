#include "RoughConductorBsdf.hpp"
#include "Microfacet.hpp"
#include "ComplexIor.hpp"
#include "Fresnel.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

RoughConductorBsdf::RoughConductorBsdf()
: _distribution("ggx"),
  _materialName("Cu"),
  _roughness(std::make_shared<ConstantTexture>(0.1f)),
  _eta(0.200438f, 0.924033f, 1.10221f),
  _k(3.91295f, 2.45285f, 2.14219f)
{
    _lobes = BsdfLobes::GlossyReflectionLobe;
}

bool RoughConductorBsdf::lookupMaterial()
{
    return ComplexIorList::lookup(_materialName, _eta, _k);
}

void RoughConductorBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    if (value.getField("eta", _eta) && value.getField("k", _k))
        _materialName.clear();
    _distribution = value["distribution"];
    if (value.getField("material", _materialName) && !lookupMaterial())
        value.parseError(tfm::format("Unable to find material with name '%s'", _materialName));

    if (auto roughness = value["roughness"])
        _roughness = scene.fetchTexture(roughness, TexelConversion::REQUEST_AVERAGE);
}

rapidjson::Value RoughConductorBsdf::toJson(Allocator &allocator) const
{
    JsonObject result{Bsdf::toJson(allocator), allocator,
        "type", "rough_conductor",
        "distribution", _distribution.toString(),
        "roughness", *_roughness
    };
    if (_materialName.empty())
        result.add("eta", _eta, "k", _k);
    else
        result.add("material", _materialName);

    return result;
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

    Vec3f m = Microfacet::sample(_distribution, sampleAlpha, event.sampler->next2D());
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

bool RoughConductorBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return false;

    float roughness = (*_roughness)[*event.info].x();
    float alpha = Microfacet::roughnessToAlpha(_distribution, roughness);

    Vec3d m = (Vec3d(event.wi) + Vec3d(event.wo)).normalized();
    sampler.put2D(Microfacet::invert(_distribution, alpha, m, sampler.untracked1D()));

    return true;
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

}


