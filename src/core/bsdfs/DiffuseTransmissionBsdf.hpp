#ifndef DIFFUSETRANSMISSIONBSDF_HPP_
#define DIFFUSETRANSMISSIONBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;
struct SurfaceScatterEvent;

class DiffuseTransmissionBsdf : public Bsdf
{
    float _transmittance;

public:
    DiffuseTransmissionBsdf();

    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    float transmittance() const
    {
        return _transmittance;
    }

    void setTransmittance(float transmittance)
    {
        _transmittance = transmittance;
    }
};

}

#endif /* DIFFUSETRANSMISSIONBSDF_HPP_ */
