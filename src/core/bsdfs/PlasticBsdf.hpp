#ifndef PLASTICBSDF_HPP_
#define PLASTICBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class PlasticBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;

    float _diffuseFresnel;
    float _avgTransmittance;
    Vec3f _scaledSigmaA;

public:
    PlasticBsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    float ior() const
    {
        return _ior;
    }

    float thickness() const
    {
        return _thickness;
    }

    Vec3f sigmaA() const
    {
        return _sigmaA;
    }

    void setIor(float ior)
    {
        _ior = ior;
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


#endif /* DIELECTRICBSDF_HPP_ */
