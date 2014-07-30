#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "extern/sobol/sobol.h"
#include "extern/SFMT/SFMT.h"

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

#include "Debug.hpp"

namespace Tungsten
{

class SampleGenerator
{
public:
    virtual ~SampleGenerator() {}

    virtual void setup(uint32 pixelId, int sample) = 0;

    virtual uint32 nextI() = 0;

    virtual float next1D()
    {
        return BitManip::normalizedUint(nextI());
    }

    virtual Vec2f next2D()
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    virtual Vec3f next3D()
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        return Vec3f(a, b, c);
    }

    virtual Vec4f next4D()
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        float d = next1D();
        return Vec4f(a, b, c, d);
    }
};

class SobolSampler : public SampleGenerator
{
    uint32 _scramble;
    uint32 _index;
    int _dimension;

    inline uint32 permutedIndex() const
    {
        return (_index & ~0xFF) | ((_index + _scramble) & 0xFF);
    }

public:
    SobolSampler()
    : _scramble(0), _index(0), _dimension(0)
    {
    }

    void setup(uint32 pixelId, int sample) override final
    {
        _scramble = MathUtil::hash32(pixelId);
        _index = sample;
        _dimension = 0;
    }

    uint32 nextI() override final
    {
        if (_dimension >= 1024)
            FAIL("Sobol sampler dimension > 1024!");
        return sobol::sample(permutedIndex(), _dimension++, _scramble);
    }
};

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

#endif /* SAMPLEGENERATOR_HPP_ */
