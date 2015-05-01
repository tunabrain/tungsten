#ifndef LENSSAMPLE_HPP_
#define LENSSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class SampleGenerator;
class Medium;

struct LensSample
{
    SampleGenerator *sampler;
    Vec3f p;

    Vec2u pixel;
    Vec3f d;
    float dist;
    float pdf;
    Vec3f weight;
    const Medium *medium;

    LensSample(SampleGenerator *sampler_)
    : sampler(sampler_)
    {
    }

    LensSample(SampleGenerator *sampler_, const Vec3f &p_)
    : sampler(sampler_), p(p_)
    {
    }
};

}

#endif /* LENSSAMPLE_HPP_ */
