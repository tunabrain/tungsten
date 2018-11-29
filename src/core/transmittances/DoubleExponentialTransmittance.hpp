#ifndef DoubleExponentialTransmittance_HPP_
#define DoubleExponentialTransmittance_HPP_

#include "Transmittance.hpp"

namespace Tungsten {

class DoubleExponentialTransmittance : public Transmittance
{
    float _sigmaA;
    float _sigmaB;

public:
    DoubleExponentialTransmittance();

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



#endif /* DoubleExponentialTransmittance_HPP_ */
