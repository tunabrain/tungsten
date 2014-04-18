#ifndef INTEGRATOR_HPP_
#define INTEGRATOR_HPP_

#include "sampling/SampleGenerator.hpp"

#include "math/Vec.hpp"

namespace Tungsten
{

class Camera;

class Integrator
{
protected:
    virtual ~Integrator()
    {
    }

public:
    virtual Vec3f traceSample(const Camera &cam, Vec2u pixel, SampleGenerator &sampler) = 0;
};

}

#endif /* INTEGRATOR_HPP_ */
