#ifndef ROUGHCOATBSDF_HPP_
#define ROUGHCOATBSDF_HPP_

#include "Bsdf.hpp"
#include "Microfacet.hpp"

namespace Tungsten {

class RoughCoatBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;
    std::shared_ptr<Bsdf> _substrate;
    std::string _distributionName;
    std::shared_ptr<Texture> _roughness;

    float _avgTransmittance;
    Vec3f _scaledSigmaA;
    Microfacet::Distribution _distribution;

public:
    RoughCoatBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    void substrateEvalAndPdf(const SurfaceScatterEvent &event, float eta,
            float Fi, float cosThetaTi, float &pdf, Vec3f &brdf) const;

    //TODO transmissive substrate
    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    const std::string &distributionName() const
    {
        return _distributionName;
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

    const std::shared_ptr<Bsdf> &substrate() const
    {
        return _substrate;
    }

    float thickness() const
    {
        return _thickness;
    }

    void setDistributionName(const std::string &distributionName)
    {
        _distributionName = distributionName;
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

    void setSubstrate(const std::shared_ptr<Bsdf> &substrate)
    {
        _substrate = substrate;
    }

    void setThickness(float thickness)
    {
        _thickness = thickness;
    }
};

}

#endif /* ROUGHCOATBSDF_HPP_ */
