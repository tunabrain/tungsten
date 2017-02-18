#ifndef ISOTROPICPHASEFUNCTION_HPP_
#define ISOTROPICPHASEFUNCTION_HPP_

#include "PhaseFunction.hpp"

namespace Tungsten {

class IsotropicPhaseFunction : public PhaseFunction
{
public:
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const Vec3f &wi, const Vec3f &wo) const override;
    virtual bool sample(PathSampleGenerator &sampler, const Vec3f &wi, PhaseSample &sample) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const Vec3f &wi, const Vec3f &wo) const;
    virtual float pdf(const Vec3f &wi, const Vec3f &wo) const override;
};

}

#endif /* ISOTROPICPHASEFUNCTION_HPP_ */
