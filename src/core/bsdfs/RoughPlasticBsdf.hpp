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
    Vec3f _sigmaA;
    std::string _distributionName;
    std::shared_ptr<Texture> _roughness;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;
    Microfacet::Distribution _distribution;

    void init();

public:
    RoughPlasticBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    float ior() const
    {
        return _ior;
    }
};

}


#endif /* ROUGHPLASTICBSDF_HPP_ */
