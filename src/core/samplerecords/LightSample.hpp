#ifndef LIGHTSAMPLE_HPP_
#define LIGHTSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten
{

class SampleGenerator;

struct LightSample
{
    SampleGenerator *sampler;
    Vec3f p;

    Vec3f d;
    float dist;
    float pdf;

    LightSample(SampleGenerator *sampler_, const Vec3f &p_)
    : sampler(sampler_), p(p_)
    {
    }
};

}

#endif /* LIGHTSAMPLE_HPP_ */
