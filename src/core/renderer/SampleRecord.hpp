#ifndef SAMPLERECORD_HPP_
#define SAMPLERECORD_HPP_

#include "math/MathUtil.hpp"
#include "math/Vec.hpp"

namespace Tungsten {

struct SampleRecord
{
    uint32 sampleCount, nextSampleCount, sampleIndex;
    float adaptiveWeight;
    float mean, runningVariance;

    SampleRecord()
    : sampleCount(0), nextSampleCount(0), sampleIndex(0),
      adaptiveWeight(0.0f),
      mean(0.0f), runningVariance(0.0f)
    {
    }

    inline void addSample(float x)
    {
        sampleCount++;
        float delta = x - mean;
        mean += delta/sampleCount;
        runningVariance += delta*(x - mean);
    }

    inline void addSample(const Vec3f &x)
    {
        addSample(x.luminance());
    }

    inline float variance() const
    {
        return runningVariance/(sampleCount - 1);
    }

    inline float errorEstimate() const
    {
        return variance()/(sampleCount*max(mean*mean, 1e-3f));
    }
};

}

#endif /* SAMPLERECORD_HPP_ */
