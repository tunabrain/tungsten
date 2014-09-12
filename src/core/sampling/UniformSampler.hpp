#ifndef UNIFORMSAMPLER_HPP_
#define UNIFORMSAMPLER_HPP_

#include "SampleGenerator.hpp"

#include "extern/SFMT/SFMT.h"

namespace Tungsten
{

class UniformSampler : public SampleGenerator
{
    SFMT_T _state;

public:
    UniformSampler()
    : UniformSampler(0xBA5EBA11)
    {
    }

    UniformSampler(uint32 seed)
    {
        sfmt_init_gen_rand(&_state, seed);
    }

    void setup(uint32 /*pixelId*/, int /*sample*/) override final
    {
    }

    uint32 nextI() override final
    {
        return sfmt_genrand_uint32(&_state);
    }

    virtual float next1D()
    {
        return BitManip::normalizedUint(nextI());
    }
};

}

#endif /* UNIFORMSAMPLER_HPP_ */
