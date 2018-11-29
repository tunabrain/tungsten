#ifndef INTERPOLATEDTRANSMITTANCE_HPP_
#define INTERPOLATEDTRANSMITTANCE_HPP_

#include "Transmittance.hpp"

#include <vector>
#include <memory>

namespace Tungsten {

class InterpolatedTransmittance : public Transmittance
{
    std::shared_ptr<Transmittance> _trA;
    std::shared_ptr<Transmittance> _trB;
    float _u;

public:
    InterpolatedTransmittance();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f surfaceSurface(const Vec3f &tau) const override final;
    virtual Vec3f surfaceMedium(const Vec3f &tau) const override final;
    virtual Vec3f mediumSurface(const Vec3f &tau) const override final;
    virtual Vec3f mediumMedium(const Vec3f &tau) const override final;

    virtual float sigmaBar() const override final;

    virtual float sampleSurface(PathSampleGenerator &sampler) const override final;
    virtual float sampleMedium(PathSampleGenerator &sampler) const override final;
};


}



#endif /* INTERPOLATEDTRANSMITTANCE_HPP_ */
