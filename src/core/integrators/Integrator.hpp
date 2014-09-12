#ifndef INTEGRATOR_HPP_
#define INTEGRATOR_HPP_

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten
{

class Camera;
class TraceableScene;
class SampleGenerator;
class UniformSampler;

class Integrator : public JsonSerializable
{
public:
    virtual ~Integrator()
    {
    }

    virtual Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler) = 0;
    virtual Integrator *cloneThreadSafe(uint32 threadId, const TraceableScene *scene) const = 0;
};

}

#endif /* INTEGRATOR_HPP_ */
