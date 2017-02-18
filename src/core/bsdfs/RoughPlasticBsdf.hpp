#ifndef ROUGHPLASTICBSDF_HPP_
#define ROUGHPLASTICBSDF_HPP_

#include "Bsdf.hpp"
#include "Microfacet.hpp"

namespace Tungsten {

class Scene;

class RoughPlasticBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    float _substrateWeight;
    Vec3f _sigmaA;
    Microfacet::Distribution _distribution;
    std::shared_ptr<Texture> _roughness;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;

public:
    RoughPlasticBsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    const char *distributionName() const
    {
        return _distribution.toString();
    }

    float ior() const
    {
        return _ior;
    }

    const std::shared_ptr<Texture> &roughness() const
    {
        return _roughness;
    }

    Vec3f sigmaA() const
    {
        return _sigmaA;
    }

    float thickness() const
    {
        return _thickness;
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

    void setSigmaA(Vec3f sigmaA)
    {
        _sigmaA = sigmaA;
    }

    void setThickness(float thickness)
    {
        _thickness = thickness;
    }
};

}


#endif /* ROUGHPLASTICBSDF_HPP_ */
