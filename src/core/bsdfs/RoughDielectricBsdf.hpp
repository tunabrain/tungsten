#ifndef ROUGHDIELECTRICBSDF_HPP_
#define ROUGHDIELECTRICBSDF_HPP_

#include "Microfacet.hpp"
#include "Bsdf.hpp"

namespace Tungsten {

class RoughDielectricBsdf : public Bsdf
{
    Microfacet::Distribution _distribution;

    std::shared_ptr<Texture> _roughness;
    float _ior;
    float _invIor;
    bool _enableT;


public:
    RoughDielectricBsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    static bool sampleBase(SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    static Vec3f evalBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    static bool invertBase(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event,
            bool sampleR, bool sampleT, float roughness, float ior, Microfacet::Distribution distribution);

    static float pdfBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;
    virtual float eta(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    const char *distributionName() const
    {
        return _distribution.toString();
    }

    float ior() const
    {
        return _ior;
    }

    bool enableTransmission() const
    {
        return _enableT;
    }

    const std::shared_ptr<Texture> &roughness() const
    {
        return _roughness;
    }

    void setDistributionName(const std::string &distributionName)
    {
        _distribution = distributionName;
    }

    void setIor(float ior)
    {
        _ior = ior;
    }

    void setRoughness(const std::shared_ptr<Texture> &roughness)
    {
        _roughness = roughness;
    }

    void setEnableTransmission(bool enableTransmission)
    {
        _enableT = enableTransmission;
    }
};

}


#endif /* ROUGHDIELECTRICBSDF_HPP_ */
