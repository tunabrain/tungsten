#ifndef PHASEFUNCTION_HPP_
#define PHASEFUNCTION_HPP_

#include "samplerecords/PhaseSample.hpp"

#include "sampling/WritablePathSampleGenerator.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten {

class PathSampleGenerator;
class Scene;

class PhaseFunction : public JsonSerializable
{
public:
    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const Vec3f &wi, const Vec3f &wo) const = 0;
    virtual bool sample(PathSampleGenerator &sampler, const Vec3f &wi, PhaseSample &sample) const = 0;
    virtual bool invert(WritablePathSampleGenerator &sampler, const Vec3f &wi, const Vec3f &wo) const;
    virtual float pdf(const Vec3f &wi, const Vec3f &wo) const = 0;
};

}

#endif /* PHASEFUNCTION_HPP_ */
