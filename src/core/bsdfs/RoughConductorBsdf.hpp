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

    void init();

public:
    RoughConductorBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    const Vec3f& eta() const {
        return _eta;
    }

    const Vec3f& k() const {
        return _k;
    }

    const std::shared_ptr<Texture> &roughness() const {
        return _roughness;
    }

    const std::string &distributionName() const {
        return _distributionName;
    }
};

}

#endif /* ROUGHCONDUCTORBSDF_HPP_ */
