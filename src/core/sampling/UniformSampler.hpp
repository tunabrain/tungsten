#ifndef UNIFORMSAMPLER_HPP_
#define UNIFORMSAMPLER_HPP_

#include "SampleGenerator.hpp"

#include <SFMT/SFMT.h>

namespace Tungsten {

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

    virtual void saveState(OutputStreamHandle &out) override final
    {
        FileUtils::streamWrite(out, _state);
    }

    virtual void loadState(InputStreamHandle &in)  override final
    {
        FileUtils::streamRead(in, _state);
    }

    virtual void setup(uint32 /*pixelId*/, int /*sample*/) override final
    {
    }

    inline uint32 nextI()
    {
        return sfmt_genrand_uint32(&_state);
    }

    inline virtual float next1D() override final
    {
        return BitManip::normalizedUint(nextI());
    }

    inline virtual Vec2f next2D() override final
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    inline virtual Vec3f next3D() override final
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        return Vec3f(a, b, c);
    }

    inline virtual Vec4f next4D() override final
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        float d = next1D();
        return Vec4f(a, b, c, d);
    }
};

}

#endif /* UNIFORMSAMPLER_HPP_ */
