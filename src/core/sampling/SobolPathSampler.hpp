#ifndef SOBOLPATHSAMPLER_HPP_
#define SOBOLPATHSAMPLER_HPP_

#include "PathSampleGenerator.hpp"
#include "UniformSampler.hpp"

#include "Debug.hpp"

#include <sobol/sobol.h>
#include <cstring>

namespace Tungsten {

class SobolPathSampler : public PathSampleGenerator
{
    UniformSampler _supplementalSampler;
    uint32 _seed;
    uint32 _scramble;
    uint32 _index;
    int _pathOffset;
    int _blockOffset[ContinuousBlockSize];

    inline uint32 permutedIndex() const
    {
        return (_index & ~0xFF) | ((_index + _scramble) & 0xFF);
    }

    int blockMaximum(SampleBlock block)
    {
        switch (block) {
        case EmitterSample:
            return 6;
        case MediumTransmittanceSample:
            return 1;
        case DiscreteEmitterSample:
            return 1;
        case DiscreteTransmittanceSample:
            return 1;
        case DiscreteTransparencySample:
            return 1;
        case DiscreteRouletteSample:
            return 1;
        }
        return 0;
    }

public:
    SobolPathSampler(uint32 seed)
    : _supplementalSampler(seed),
      _seed(seed),
      _scramble(0),
      _index(0),
      _pathOffset(0)
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

    virtual void startPath(uint32 pixelId, int sample) override final
    {
        _scramble = _seed ^ MathUtil::hash32(pixelId);
        _index = sample;
        _pathOffset = 0;
        std::memset(_blockOffset, 0, ContinuousBlockSize*sizeof(int));
    }

    virtual void advancePath() override final
    {
        _pathOffset += ContinuousBlockSize;
        std::memset(_blockOffset, 0, ContinuousBlockSize*sizeof(int));
    }

    virtual bool nextBoolean(SampleBlock /*block*/, float pTrue) override final
    {
        return _supplementalSampler.next1D() < pTrue;
    }

    virtual int nextDiscrete(SampleBlock /*block*/, int numChoices) override final
    {
        return int(_supplementalSampler.next1D()*numChoices);
    }

    virtual float next1D(SampleBlock block) override final
    {
        if (_blockOffset[block] > blockMaximum(block))
            FAIL("_blockOffset[%s] (%d) > %d", block, _blockOffset[block], blockMaximum(block));
        int dimension = _pathOffset + block + _blockOffset[block];
        _blockOffset[block]++;
        if (dimension >= 1024)
            return _supplementalSampler.next1D();
        return BitManip::normalizedUint(sobol::sample(permutedIndex(), dimension, _scramble));
    }

    inline virtual Vec2f next2D(SampleBlock block) override final
    {
        float a = next1D(block);
        float b = next1D(block);
        return Vec2f(a, b);
    }
};

}

#endif /* SOBOLPATHSAMPLER_HPP_ */
