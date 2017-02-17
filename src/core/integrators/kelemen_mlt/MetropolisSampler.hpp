#ifndef METROPOLISSAMPLER_HPP_
#define METROPOLISSAMPLER_HPP_

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "Debug.hpp"

#include <memory>
#include <cmath>

namespace Tungsten {

class MetropolisSampler : public PathSampleGenerator
{
    struct SampleRecord
    {
        float value;
        int time;
    };
    struct StackEntry
    {
        SampleRecord sample;
        int idx;
    };

    UniformSampler *_helperGenerator;
    std::unique_ptr<SampleRecord[]> _sampleVector;
    std::unique_ptr<StackEntry[]> _sampleStack;
    int _maxSize;
    int _vectorIdx;
    int _stackIdx;
    int _currentTime;
    int _largeStepTime;

    bool _largeStep;

    void push(int idx)
    {
        _sampleStack[_stackIdx++] = StackEntry{_sampleVector[idx], idx};
    }

    inline float mutate(float value)
    {
        const float S1 = 1.0f/1024.0f;
        const float S2 = 1.0f/64.0f;
        const float Factor = -std::log(S2/S1);

        float random = _helperGenerator->next1D();
        bool negative = random < 0.5f;
        random = negative ? random*2.0f : (random - 0.5f)*2.0f;

        float delta = S2*std::exp(Factor*random);
        if (negative) {
            value -= delta;
            if (value < 0.0f)
                value += 1.0f;
        } else {
            value += delta;
            if (value >= 1.0f)
                value -= 1.0f;
        }
        if (value == 1.0f)
            value = 0.0f;
        return value;
    }

public:
    MetropolisSampler(UniformSampler *helperGenerator, int maxVectorSize)
    : _helperGenerator(helperGenerator),
      _sampleVector(new SampleRecord[maxVectorSize]),
      _sampleStack(new StackEntry[maxVectorSize]),
      _maxSize(maxVectorSize),
      _vectorIdx(0),
      _stackIdx(0),
      _currentTime(1),
      _largeStepTime(0),
      _largeStep(true)
    {
        std::memset(_sampleVector.get(), 0, maxVectorSize*sizeof(SampleRecord));
    }

    virtual void startPath(uint32 /*pixelId*/, uint32 /*sample*/) override
    {
    }
    virtual void advancePath()
    {
    }

    virtual void saveState(OutputStreamHandle &/*out*/) override
    {
    }
    virtual void loadState(InputStreamHandle &/*in*/) override
    {
    }

    void setHelperGenerator(UniformSampler *generator)
    {
        _helperGenerator = generator;
    }

    void setLargeStep(bool step)
    {
        _largeStep = step;
    }

    void accept()
    {
        if (_largeStep)
            _largeStepTime = _currentTime;
        _currentTime++;
        _vectorIdx = 0;
        _stackIdx = 0;
    }

    void reject()
    {
        for (int i = 0; i < _stackIdx; ++i)
            _sampleVector[_sampleStack[i].idx] = _sampleStack[i].sample;
        _vectorIdx = 0;
        _stackIdx = 0;
    }

    void setRandomElement(int idx, float value)
    {
        _sampleVector[idx].value = value;
        _sampleVector[idx].time = _currentTime;
    }

    virtual bool nextBoolean(float pTrue) override final
    {
        return next1D() < pTrue;
    }
    virtual int nextDiscrete(int numChoices) override final
    {
        return int(next1D()*numChoices);
    }

    inline virtual float next1D() override final
    {
        if (_vectorIdx == _maxSize)
            FAIL("Exceeded maximum size of metropolis sampler");

        int idx = _vectorIdx++;
        SampleRecord &sample = _sampleVector[idx];
        if (sample.time < _currentTime) {
            if (_largeStep) {
                push(idx);
                sample.value = _helperGenerator->next1D();
            } else {
                if (sample.time < _largeStepTime) {
                    sample.value = _helperGenerator->next1D();
                    sample.time = _largeStepTime;
                }
                for (int i = sample.time + 1; i < _currentTime; ++i)
                    sample.value = mutate(sample.value);
                sample.time = _currentTime - 1;
                push(idx);
                sample.value = mutate(sample.value);
            }
            sample.time = _currentTime;
        }
        return sample.value;
    }

    inline virtual Vec2f next2D() override final
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    virtual UniformSampler &uniformGenerator() override final
    {
        return *_helperGenerator;
    }
};

}

#endif /* METROPOLISSAMPLER_HPP_ */
