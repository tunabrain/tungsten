#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "extern/sobol/sobol.h"

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

#include "Config.hpp"

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
        return sobol::sample(_index, _dimension++, _scramble) + _scramble;
    }
};

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

}

#endif /* SAMPLEGENERATOR_HPP_ */
