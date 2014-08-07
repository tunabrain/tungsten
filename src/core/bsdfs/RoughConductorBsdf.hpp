#ifndef ROUGHCONDUCTORBSDF_HPP_
#define ROUGHCONDUCTORBSDF_HPP_

#include "Microfacet.hpp"
#include "Fresnel.hpp"
#include "Bsdf.hpp"

#include "sampling/SampleGenerator.hpp"

namespace Tungsten
{

class RoughConductorBsdf : public Bsdf
{
    std::string _distributionName;
    std::string _materialName;
    std::shared_ptr<TextureA> _roughness;
    Vec3f _eta;
    Vec3f _k;

    Microfacet::Distribution _distribution;

    void init()
    {
        _distribution = Microfacet::stringToType(_distributionName);
    }

public:

    RoughConductorBsdf()
    : _distributionName("ggx"),
      _materialName("Cu"),
      _roughness(std::make_shared<ConstantTextureA>(0.1f)),
      _eta(0.200438f, 0.924033f, 1.10221f),
      _k(3.91295f, 2.45285f, 2.14219f)
    {
        _lobes = BsdfLobes::GlossyReflectionLobe;
        init();
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return false;
        if (event.wi.z() <= 0.0f)
            return false;

        //float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(event.wi.z())))*_roughness;
        float roughness = (*_roughness)[event.info->uv];
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
        event.throughput = albedo(event.info)*(F*weight);
        event.sampledLobe = BsdfLobes::GlossyReflectionLobe;
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);

        float roughness = (*_roughness)[event.info->uv];
        float alpha = Microfacet::roughnessToAlpha(_distribution, roughness);

        Vec3f hr = (event.wi + event.wo).normalized();
        float cosThetaM = event.wi.dot(hr);
        Vec3f F = Fresnel::conductorReflectance(_eta, _k, cosThetaM);
        float G = Microfacet::G(_distribution, alpha, event.wi, event.wo, hr);
        float D = Microfacet::D(_distribution, alpha, hr);
        float fr = (G*D*0.25f)/event.wi.z();

        return albedo(event.info)*(F*fr);
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;

        //float sampleRoughness = (1.2f - 0.2f*std::sqrt(event.wi.z()))*_roughness;
        float roughness = (*_roughness)[event.info->uv];
        float sampleRoughness = roughness;
        float sampleAlpha = Microfacet::roughnessToAlpha(_distribution, sampleRoughness);

        Vec3f hr = (event.wi + event.wo).normalized();
        return Microfacet::pdf(_distribution, sampleAlpha, hr)*0.25f/event.wi.dot(hr);
    }

    const Vec3f& eta() const {
        return _eta;
    }

    const Vec3f& k() const {
        return _k;
    }

    const std::shared_ptr<TextureA> &roughness() const
            {
        return _roughness;
    }

    const std::string &distributionName() const
    {
        return _distributionName;
    }
};

}

#endif /* ROUGHCONDUCTORBSDF_HPP_ */
