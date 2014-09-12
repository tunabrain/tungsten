#ifndef SMOOTHCOATBSDF_HPP_
#define SMOOTHCOATBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class SmoothCoatBsdf : public Bsdf
{
    float _ior;
    float _thickness;
    Vec3f _sigmaA;
    std::shared_ptr<Bsdf> _substrate;

    float _avgTransmittance;
    Vec3f _scaledSigmaA;

    void init();

public:
    SmoothCoatBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    //TODO transmissive substrate
    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    float ior() const
    {
        return _ior;
    }

    const Vec3f &sigmaA() const
    {
        return _sigmaA;
    }

    std::shared_ptr<Bsdf> &substrate()
    {
        return _substrate;
    }

    const std::shared_ptr<Bsdf> &substrate() const
    {
        return _substrate;
    }

    float thickness() const
    {
        return _thickness;
    }
};

}

#endif /* SMOOTHCOATBSDF_HPP_ */
