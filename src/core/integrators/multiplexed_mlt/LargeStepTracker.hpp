#ifndef LARGESTEPTRACKER_HPP_
#define LARGESTEPTRACKER_HPP_

#include "math/MathUtil.hpp"

#include "IntTypes.hpp"

namespace Tungsten {

class LargeStepTracker
{
    double _cumulativeLuminance;
    uint64 _numLargeSteps;

public:
    LargeStepTracker() : _cumulativeLuminance(0.0), _numLargeSteps(0) {}

    void add(double luminance)
    {
        _cumulativeLuminance += luminance;
        _numLargeSteps++;
    }
    LargeStepTracker &operator+=(const LargeStepTracker &o)
    {
        _cumulativeLuminance += o._cumulativeLuminance;
        _numLargeSteps += o._numLargeSteps;
        return *this;
    }

    double getAverage() const
    {
        return _cumulativeLuminance/max(_numLargeSteps, uint64(1));
    }

    double getSum() const
    {
        return _cumulativeLuminance;
    }

    uint64 getSampleCount() const
    {
        return _numLargeSteps;
    }

    void setSampleCount(uint64 numSamples)
    {
        _numLargeSteps = numSamples;
    }

    void clear()
    {
        _cumulativeLuminance = 0.0f;
        _numLargeSteps = 0;
    }
};

}

#endif /* LARGESTEPTRACKER_HPP_ */
