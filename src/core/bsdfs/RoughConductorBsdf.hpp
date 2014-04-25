#ifndef ROUGHCONDUCTORBSDF_HPP_
#define ROUGHCONDUCTORBSDF_HPP_

#include "Microfacet.hpp"
#include "Fresnel.hpp"
#include "Bsdf.hpp"

namespace Tungsten
{

class RoughConductorBsdf : public Bsdf
{
    static constexpr Microfacet::Distribution distribution = Microfacet::GGX;

    float _roughness;
    Vec3f _eta;
    Vec3f _k;

public:

    RoughConductorBsdf()
    : _roughness(0.1f),
      _eta(0.214000f, 0.950375f, 1.177500f),
      _k(3.670000f, 2.576500f, 2.160063f)
    {
        _lobes = BsdfLobes::GlossyReflectionLobe;
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "roughness", _roughness);
        JsonUtils::fromJson(v, "eta", _eta);
        JsonUtils::fromJson(v, "k", _k);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "rough_conductor", allocator);
        v.AddMember("roughness", _roughness, allocator);
        v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
        v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return false;
        if (event.wi.z() <= 0.0f)
            return false;

        float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(event.wi.z())))*_roughness;
        float alpha = Microfacet::roughnessToAlpha(distribution, _roughness);
        float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

        Vec3f m = Microfacet::sample(distribution, sampleAlpha, event.sampler.next2D());
        float wiDotM = event.wi.dot(m);
        event.wo = 2.0f*wiDotM*m - event.wi;
        if (wiDotM <= 0.0f || event.wo.z() <= 0.0f)
            return false;
        float G = Microfacet::G(distribution, alpha, event.wi, event.wo, m);
        float D = Microfacet::D(distribution, alpha, m);
        float mPdf = Microfacet::pdf(distribution, sampleAlpha, m);
        float pdf = mPdf*0.25f/wiDotM;
        float weight = wiDotM*G*D/(event.wi.z()*mPdf);
        Vec3f F = Fresnel::conductorReflectance(_eta, _k, wiDotM);

        event.pdf = pdf;
        event.throughput = base(event.info)*(F*weight);
        event.sampledLobe = BsdfLobes::GlossyReflectionLobe;
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);

        float alpha = Microfacet::roughnessToAlpha(distribution, _roughness);

        Vec3f hr = (event.wi + event.wo).normalized();
        float cosThetaM = event.wi.dot(hr);
        Vec3f F = Fresnel::conductorReflectance(_eta, _k, cosThetaM);
        float G = Microfacet::G(distribution, alpha, event.wi, event.wo, hr);
        float D = Microfacet::D(distribution, alpha, hr);
        float fr = (G*D*0.25f)/event.wi.z();

        return base(event.info)*(F*fr);
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;

        float sampleRoughness = (1.2f - 0.2f*std::sqrt(event.wi.z()))*_roughness;
        float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

        Vec3f hr = (event.wi + event.wo).normalized();
        return Microfacet::pdf(distribution, sampleAlpha, hr)*0.25f/event.wi.dot(hr);
    }
};

}

#endif /* ROUGHCONDUCTORBSDF_HPP_ */
