#ifndef SOBOLPATHSAMPLER_HPP_
#define SOBOLPATHSAMPLER_HPP_

#include "PathSampleGenerator.hpp"
#include "UniformSampler.hpp"

#include <sobol/sobol.h>
#include <cstring>

namespace Tungsten {

class SobolPathSampler : public PathSampleGenerator
{
    UniformSampler _supplementalSampler;
    uint32 _seed;
    uint32 _scramble;
    uint32 _index;
    uint32 _dimension;

    inline uint32 permutedIndex() const
    {
        return (_index & ~0xFF) | ((_index + _scramble) & 0xFF);
    }

public:
    SobolPathSampler(uint32 seed)
    : _supplementalSampler(seed),
      _seed(seed),
      _scramble(0),
      _index(0),
      _dimension(0)
    {
    }

    virtual void saveState(OutputStreamHandle &out) override final
    {
        FileUtils::streamWrite(out, _seed);
        _supplementalSampler.saveState(out);
    }

    virtual void loadState(InputStreamHandle &in)  override final
    {
        FileUtils::streamRead(in, _seed);
        _supplementalSampler.loadState(in);
    }

    virtual void startPath(uint32 pixelId, uint32 sample) override final
    {
        _scramble = _seed ^ MathUtil::hash32(pixelId);
        _index = sample;
        _dimension = 0;
    }
    virtual void advancePath() override final
    {
    }

    virtual bool nextBoolean(float pTrue) override final
    {
        return _supplementalSampler.next1D() < pTrue;
    }

    virtual int nextDiscrete(int numChoices) override final
    {
        return int(_supplementalSampler.next1D()*numChoices);
    }

    virtual float next1D() override final
    {
        if (_dimension >= 1024)
            return _supplementalSampler.next1D();
        return BitManip::normalizedUint(sobol::sample(permutedIndex(), _dimension++, _scramble));
    }

    inline virtual Vec2f next2D() override final
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    virtual UniformSampler &uniformGenerator() override final
    {
        return _supplementalSampler;
    }
};

}

#endif /* SOBOLPATHSAMPLER_HPP_ */
