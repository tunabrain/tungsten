#ifndef PLASTICBSDF_HPP_
#define PLASTICBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class Scene;

class PlasticBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;

    void init();

public:
    PlasticBsdf();

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


#endif /* DIELECTRICBSDF_HPP_ */
