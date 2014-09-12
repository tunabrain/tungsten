#ifndef ROUGHDIELECTRICBSDF_HPP_
#define ROUGHDIELECTRICBSDF_HPP_

#include "Microfacet.hpp"
#include "Fresnel.hpp"
#include "Bsdf.hpp"

#include "sampling/UniformSampler.hpp"

namespace Tungsten {

class RoughDielectricBsdf : public Bsdf
{
    std::string _distributionName;

    std::shared_ptr<TextureA> _roughness;
    float _ior;
    bool _enableT;

    Microfacet::Distribution _distribution;

    void init()
    {
        _distribution = Microfacet::stringToType(_distributionName);
    }

    // Signum that exludes 0
    template <typename T>
    static inline T sgnE(T val) {
        return val < T(0) ? T(-1) : T(1);
    }

public:

    RoughDielectricBsdf()
    : _distributionName("ggx"),
      _roughness(std::make_shared<ConstantTextureA>(0.1f)),
      _ior(1.5f),
      _enableT(true)
    {
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
        init();
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    static bool sampleBase(SurfaceScatterEvent &event, bool sampleR, bool sampleT,
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
            reflect = event.supplementalSampler->next1D() < F;
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
        event.throughput = Vec3f(std::abs(wiDotM)*G*D/(std::abs(wiDotN)*pm));

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
                event.throughput *= F;
            else
                event.throughput *= 1.0f - F;
        }

        return true;
    }

    static Vec3f evalBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
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

    static float pdfBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
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

    bool sample(SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
        float roughness = (*_roughness)[event.info->uv];

        return sampleBase(event, sampleR, sampleT, roughness, _ior, _distribution);
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
        float roughness = (*_roughness)[event.info->uv];

        return evalBase(event, sampleR, sampleT, roughness, _ior, _distribution);
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe) && _enableT;
        float roughness = (*_roughness)[event.info->uv];

        return pdfBase(event, sampleR, sampleT, roughness, _ior, _distribution);
    }

    float ior() const {
        return _ior;
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


#endif /* ROUGHDIELECTRICBSDF_HPP_ */
