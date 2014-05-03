#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "extern/sobol/sobol.h"
#include "extern/SFMT/SFMT.h"

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

#include "Config.hpp"
#include "Debug.hpp"

namespace Tungsten
{

class SampleGenerator
{
public:
    virtual ~SampleGenerator() {}

    virtual void setup(uint32 pixel, int sample) = 0;

    virtual uint32 nextI() = 0;

    virtual float next1D()
    {
        return BitManip::normalizedUint(nextI());
    }

    virtual Vec2f next2D()
    {
        return Vec2f(next1D(), next1D());
    }

    virtual Vec3f next3D()
    {
        return Vec3f(next1D(), next1D(), next1D());
    }

    virtual Vec4f next4D()
    {
        return Vec4f(next1D(), next1D(), next1D(), next1D());
    }
};

class SobolSampler : public SampleGenerator
{
    uint32 _scramble;
    uint32 _index;
    int _dimension;

    inline uint32 invert4Bit(uint32 x) const
    {
        return
            ((x & 1) << 3) |
            ((x & 2) << 1) |
            ((x & 4) >> 1) |
            ((x & 8) >> 3);
    }

    inline uint32 permutedIndex() const
    {
        return (_index & ~0xFF) | ((_index + _scramble) & 0xFF);
    }

public:
    SobolSampler()
    : _scramble(0), _index(0), _dimension(0)
    {
    }

    void setup(uint32 pixel, int sample) override final
    {
        _scramble = MathUtil::hash32(pixel);
        _index = sample;
        _dimension = 0;
    }

    uint32 nextI() override final
    {
        if (_dimension >= 1024)
            FAIL("Sobol sampler dimension > 1024!");
        return sobol::sample(permutedIndex(), _dimension++, _scramble);// + _scramble;
    }
};

#if 0
class UniformSampler : public SampleGenerator
{
    uint32 _seed;

public:
    UniformSampler()
    : _seed(0xBA5EBA11)
    {
    }

    UniformSampler(uint32 seed)
    : _seed(seed)
    {
    }

    void setup(uint32 /*pixel*/, int /*sample*/) override final
    {
    }

    uint32 nextI() override final
    {
        _seed = (_seed*1103515245u + 12345u) & 0x7FFFFFFF;
        return _seed;
    }

    virtual float next1D()
    {
        return BitManip::normalizedUint(nextI() << 1);
    }
};
#else
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

    void setup(uint32 /*pixel*/, int /*sample*/) override final
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
#endif

}

#endif /* SAMPLEGENERATOR_HPP_ */
