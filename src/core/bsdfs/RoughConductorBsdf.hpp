#ifndef ROUGHCONDUCTORBSDF_HPP_
#define ROUGHCONDUCTORBSDF_HPP_

#include "Bsdf.hpp"
#include "Microfacet.hpp"

namespace Tungsten {

class RoughConductorBsdf : public Bsdf
{
    std::string _distributionName;
    std::string _materialName;
    std::shared_ptr<Texture> _roughness;
    Vec3f _eta;
    Vec3f _k;

    Microfacet::Distribution _distribution;

    void lookupMaterial();

public:
    RoughConductorBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    const std::string &distributionName() const
    {
        return _distributionName;
    }

    Vec3f eta() const
    {
        return _eta;
    }

    Vec3f k() const
    {
        return _k;
    }

    const std::string &materialName() const
    {
        return _materialName;
    }

    const std::shared_ptr<Texture> &roughness() const
    {
        return _roughness;
    }

    void setDistributionName(const std::string &distributionName)
    {
        _distributionName = distributionName;
    }

    void setEta(Vec3f eta)
    {
        _eta = eta;
    }

    void setK(Vec3f k)
    {
        _k = k;
    }

    void setMaterialName(const std::string &materialName)
    {
        _materialName = materialName;
        lookupMaterial();
    }

    void setRoughness(const std::shared_ptr<Texture> &roughness)
    {
        _roughness = roughness;
    }
};

}

#endif /* ROUGHCONDUCTORBSDF_HPP_ */
