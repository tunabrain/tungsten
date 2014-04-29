#ifndef ROUGHDIELECTRICBSDF_HPP_
#define ROUGHDIELECTRICBSDF_HPP_

#include "Microfacet.hpp"
#include "Fresnel.hpp"
#include "Bsdf.hpp"

namespace Tungsten {

class RoughDielectricBsdf : public Bsdf
{
    static constexpr Microfacet::Distribution distribution = Microfacet::Beckmann;
    float _roughness;
    float _ior;

public:

    RoughDielectricBsdf()
    : _roughness(0.1f),
      _ior(1.5f)
    {
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "roughness", _roughness);
        JsonUtils::fromJson(v, "ior", _ior);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "rough_dielectric", allocator);
        v.AddMember("roughness", _roughness, allocator);
        v.AddMember("ior", _ior, allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe);
        float wiDotN = event.wi.z();

        float eta = wiDotN < 0.0f ? _ior : 1.0f/_ior;

        float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*_roughness;
        float alpha = Microfacet::roughnessToAlpha(distribution, _roughness);
        float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

        Vec3f m = Microfacet::sample(distribution, sampleAlpha, event.sampler.next2D());
        float pm = Microfacet::pdf(distribution, sampleAlpha, m);

        if (pm < 1e-10f)
            return false;

        float wiDotM = event.wi.dot(m);
        float cosThetaT = 0.0f;
        float F = Fresnel::dielectricReflectance(1.0f/_ior, wiDotM, cosThetaT);
        float etaM = wiDotM < 0.0f ? _ior : 1.0f/_ior;

        bool reflect;
        if (sampleR && sampleT) {
            reflect = event.supplementalSampler.next1D() < F;
        } else if (sampleT) {
            if (F == 1.0f)
                return false;
            F = 0.0f;
            reflect = false;
        } else if (sampleR) {
            F = 1.0f;
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
        event.throughput = base(event.info)*std::abs(wiDotM)*G*D/(std::abs(wiDotN)*pm);

        if (reflect)
            event.pdf = F*pm*0.25f/std::abs(wiDotM);
        else
            event.pdf = (1.0f - F)*pm*std::abs(woDotM)/sqr(eta*wiDotM + woDotM);

        if (std::isnan(event.pdf)) {
            DBG("NaN!");
        }

        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe);
        float wiDotN = event.wi.z();
        float woDotN = event.wo.z();

        bool reflect = wiDotN*woDotN >= 0.0f;
        if ((reflect && !sampleR) || (!reflect && !sampleT))
            return Vec3f(0.0f);

        float alpha = Microfacet::roughnessToAlpha(distribution, _roughness);

        float eta = wiDotN < 0.0f ? _ior : 1.0f/_ior;
        Vec3f m;
        if (reflect)
            m = sgnE(wiDotN)*(event.wi + event.wo).normalized();
        else
            m = -(event.wi*eta + event.wo).normalized();
        float wiDotM = event.wi.dot(m);
        float woDotM = event.wo.dot(m);
        float F = Fresnel::dielectricReflectance(1.0f/_ior, wiDotM);
        float G = Microfacet::G(distribution, alpha, event.wi, event.wo, m);
        float D = Microfacet::D(distribution, alpha, m);
        Vec3f throughput = base(event.info);

        if (reflect) {
            float fr = (F*G*D*0.25f)/std::abs(wiDotN);
            return throughput*fr;
        } else {
            float fs = std::abs(wiDotM*woDotM)*(1.0f - F)*G*D/(sqr(eta*wiDotM + woDotM)*std::abs(wiDotN));
            return throughput*fs;
        }
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        bool sampleR = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
        bool sampleT = event.requestedLobe.test(BsdfLobes::GlossyTransmissionLobe);
        float wiDotN = event.wi.z();
        float woDotN = event.wo.z();

        bool reflect = wiDotN*woDotN >= 0.0f;
        if ((reflect && !sampleR) || (!reflect && !sampleT))
            return 0.0f;

        float sampleRoughness = (1.2f - 0.2f*std::sqrt(std::abs(wiDotN)))*_roughness;
        float sampleAlpha = Microfacet::roughnessToAlpha(distribution, sampleRoughness);

        float eta = wiDotN < 0.0f ? _ior : 1.0f/_ior;
        Vec3f m;
        if (reflect)
            m = sgnE(wiDotN)*(event.wi + event.wo).normalized();
        else
            m = -(event.wi*eta + event.wo).normalized();
        float wiDotM = event.wi.dot(m);
        float woDotM = event.wo.dot(m);
        float F = Fresnel::dielectricReflectance(1.0f/_ior, wiDotM);
        float pm = Microfacet::pdf(distribution, sampleAlpha, m);

        if (reflect)
            return F*pm*0.25f/std::abs(wiDotM);
        else
            return (1.0f - F)*pm*std::abs(woDotM)/sqr(eta*wiDotM + woDotM);
    }

    float ior() const {
        return _ior;
    }

    float roughness() const {
        return _roughness;
    }
};

}


#endif /* ROUGHDIELECTRICBSDF_HPP_ */
