#ifndef SAMPLERECORD_HPP_
#define SAMPLERECORD_HPP_

#include "math/MathUtil.hpp"
#include "math/Vec.hpp"

#include "io/FileUtils.hpp"

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

    void saveState(OutputStreamHandle &out)
    {
        FileUtils::streamWrite(out, sampleCount);
        FileUtils::streamWrite(out, nextSampleCount);
        FileUtils::streamWrite(out, sampleIndex);
        FileUtils::streamWrite(out, adaptiveWeight);
        FileUtils::streamWrite(out, mean);
        FileUtils::streamWrite(out, runningVariance);
    }

    void loadState(InputStreamHandle &in)
    {
        FileUtils::streamRead(in, sampleCount);
        FileUtils::streamRead(in, nextSampleCount);
        FileUtils::streamRead(in, sampleIndex);
        FileUtils::streamRead(in, adaptiveWeight);
        FileUtils::streamRead(in, mean);
        FileUtils::streamRead(in, runningVariance);
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
