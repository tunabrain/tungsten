#ifndef LIGHTSAMPLE_HPP_
#define LIGHTSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class PathSampleGenerator;
class Medium;

struct LightSample
{
    PathSampleGenerator *sampler;
    Vec3f p;

    Vec3f d;
    float dist;
    float pdf;
    Vec3f weight;
    const Medium *medium;

    LightSample(PathSampleGenerator *sampler_)
    : sampler(sampler_)
    {
    }
    LightSample(PathSampleGenerator *sampler_, const Vec3f &p_)
    : sampler(sampler_), p(p_)
    {
    }
};

}

#endif /* LIGHTSAMPLE_HPP_ */
