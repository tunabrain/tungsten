#ifndef RAYLEIGHPHASEFUNCTION_HPP_
#define RAYLEIGHPHASEFUNCTION_HPP_

#include "PhaseFunction.hpp"

namespace Tungsten {

class RayleighPhaseFunction : public PhaseFunction
{
    static inline float rayleigh(float cosTheta);
public:
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const Vec3f &wi, const Vec3f &wo) const override;
    virtual bool sample(PathSampleGenerator &sampler, const Vec3f &wi, PhaseSample &sample) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const Vec3f &wi, const Vec3f &wo) const;
    virtual float pdf(const Vec3f &wi, const Vec3f &wo) const override;
};

}

#endif /* RAYLEIGHPHASEFUNCTION_HPP_ */
