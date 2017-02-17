#ifndef UNIFORMPATHSAMPLER_HPP_
#define UNIFORMPATHSAMPLER_HPP_

#include "PathSampleGenerator.hpp"
#include "UniformSampler.hpp"

namespace Tungsten {

class UniformPathSampler : public PathSampleGenerator
{
    UniformSampler _sampler;

public:
    UniformPathSampler(uint32 seed)
    : _sampler(seed)
    {
    }
    UniformPathSampler(const UniformSampler &sampler)
    : _sampler(sampler)
    {
    }

    virtual void startPath(uint32 /*pixelId*/, uint32 /*sample*/) override
    {
    }
    virtual void advancePath() override
    {
    }

    virtual void saveState(OutputStreamHandle &out) override
    {
        _sampler.saveState(out);
    }
    virtual void loadState(InputStreamHandle &in) override
    {
        _sampler.loadState(in);
    }

    virtual bool nextBoolean(float pTrue) override final
    {
        return _sampler.next1D() < pTrue;
    }
    virtual int nextDiscrete(int numChoices) override final
    {
        return int(_sampler.next1D()*numChoices);
    }
    virtual float next1D() override final
    {
        return _sampler.next1D();
    }
    virtual Vec2f next2D() override final
    {
        float a = _sampler.next1D();
        float b = _sampler.next1D();
        return Vec2f(a, b);
    }

    const UniformSampler &sampler() const
    {
        return _sampler;
    }

    virtual UniformSampler &uniformGenerator() override final
    {
        return _sampler;
    }
};

}

#endif /* UNIFORMPATHSAMPLER_HPP_ */
