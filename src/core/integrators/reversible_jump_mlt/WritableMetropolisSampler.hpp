#ifndef WRITABLEMETROPOLISSAMPLER_HPP_
#define WRITABLEMETROPOLISSAMPLER_HPP_

#include "sampling/WritablePathSampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "Debug.hpp"

#include <memory>
#include <cmath>

namespace Tungsten {

class WritableMetropolisSampler : public WritablePathSampleGenerator
{
    static CONSTEXPR int FullBlockSize = 11;
    typedef Vec<float, 11> FullBlock;

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

    bool _gaussianMutation;
    UniformSampler *_helperGenerator;
    int _maxSize;
    Vec3f _stepSize;
    std::unique_ptr<SampleRecord[]> _sampleVector;
    std::unique_ptr<StackEntry[]> _sampleStack;
    int _stackIdx;
    int _currentTime;
    int _largeStepTime;

    bool _largeStep;
    bool _frozen;

    int _vertexIdx;
    int _blockOffset;

    inline void push(int idx)
    {
        _sampleStack[_stackIdx++] = StackEntry{_sampleVector[idx], idx};
    }

    inline float mutate(float value)
    {
        if (_gaussianMutation) {
            float xi = _helperGenerator->next1D();
            float s = _stepSize.z()*8.0f;
            value += s*std::atanh((xi*2.0f - 1.0f)*std::tanh(1.0f/(2.0f*s)));
            if (value  < 0.0f) value += 1.0f;
            if (value >= 1.0f) value -= 1.0f;
            return value;
        } else {
            float random = _helperGenerator->next1D();
            bool negative = random < 0.5f;
            random = negative ? random*2.0f : (random - 0.5f)*2.0f;

            float delta = _stepSize.y()*std::exp(_stepSize.x()*random);
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
    }

    void setStepSize(float r1, float r2)
    {
        _stepSize = Vec3f(-std::log(r2/r1), r2, r1);
    }

    void setStepSize(float r1)
    {
        setStepSize(r1, 16.0f*r1);
    }

public:
    WritableMetropolisSampler(bool gaussian, UniformSampler *helperGenerator, int numBounces)
    : _gaussianMutation(gaussian),
      _helperGenerator(helperGenerator),
      _maxSize(numBounces*FullBlockSize),
      _sampleVector(new SampleRecord[_maxSize]),
      _sampleStack(new StackEntry[_maxSize*4]),
      _stackIdx(0),
      _currentTime(1),
      _largeStepTime(0),
      _largeStep(true),
      _frozen(false),
      _vertexIdx(0),
      _blockOffset(0)
    {
        std::memset(_sampleVector.get(), 0, _maxSize*sizeof(SampleRecord));
        setStepSize(1.0f/1024.0f, 1.0f/64.0f);
        startPath(0, 0);
    }

    WritableMetropolisSampler(const WritableMetropolisSampler &o)
    : _gaussianMutation(o._gaussianMutation),
      _helperGenerator(o._helperGenerator),
      _maxSize(o._maxSize),
      _stepSize(o._stepSize),
      _sampleVector(new SampleRecord[_maxSize]),
      _sampleStack(new StackEntry[_maxSize*4]),
      _stackIdx(o._stackIdx),
      _currentTime(o._currentTime),
      _largeStepTime(o._largeStepTime),
      _largeStep(o._largeStep),
      _frozen(o._frozen)
    {
        std::memcpy(_sampleVector.get(), o._sampleVector.get(), _maxSize*sizeof(SampleRecord));
        std::memcpy( _sampleStack.get(),  o._sampleStack.get(), _maxSize*4*sizeof(StackEntry));
        startPath(0, 0);
    }

    inline float sech(float x) const
    {
        return 2.0f/(std::exp(x) + std::exp(-x));
    }

    inline float mutationWeight(float a, float b) const
    {
        float delta = min(std::abs(a - b), 1.0f - std::abs(a - b));
        if (_gaussianMutation) {
            float s = _stepSize.z()*8.0f;
            float norm = 2.0f*s*std::tanh(1.0f/(2.0f*s));

            float gamma = sech(delta/s);
            return (gamma*gamma)/norm;
        } else {
            if (delta < _stepSize.z() || delta > _stepSize.y())
                return 0.0f;

            return std::log(delta/_stepSize.y())/_stepSize.x();
        }
    }

    inline float mutationWeight(const FullBlock &a, const FullBlock &b) const
    {
        float result = 1.0f;
        for (int i = 0; i < FullBlockSize; ++i)
            if (a[i] != b[i])
                result *= mutationWeight(a[i], b[i]);
        return result;
    }

    inline FullBlock getFullBounce(int vertex) const
    {
        FullBlock result;
        for (int i = 0; i < FullBlockSize; ++i)
            result[i] = _sampleVector[FullBlockSize*vertex + i].value;
        return result;
    }

    inline void setFullBounce(int vertex, const FullBlock &v) const
    {
        for (int i = 0; i < FullBlockSize; ++i)
            _sampleVector[FullBlockSize*vertex + i].value = v[i];
    }

    virtual void startPath(uint32 /*pixelId*/, uint32 /*sample*/) override final
    {
        _vertexIdx = 0;
        _blockOffset = 0;
    }
    virtual void advancePath() override final
    {
        _vertexIdx++;
        _blockOffset = 0;
    }

    virtual void saveState(OutputStreamHandle &/*out*/) override final
    {
    }
    virtual void loadState(InputStreamHandle &/*in*/) override final
    {
    }

    void setHelperGenerator(UniformSampler *generator)
    {
        _helperGenerator = generator;
    }

    void freeze()
    {
        _frozen = true;
    }
    void smallStep()
    {
        _largeStep = false;
        _frozen = false;
    }

    void largeStep()
    {
        _largeStep = true;
        _frozen = false;
    }

    void accept()
    {
        if (_largeStep)
            _largeStepTime = _currentTime;
        _currentTime++;
        _stackIdx = 0;
        startPath(0, 0);
    }

    void reject()
    {
        for (int i = _stackIdx - 1; i >= 0; --i)
            _sampleVector[_sampleStack[i].idx] = _sampleStack[i].sample;
        _stackIdx = 0;
        startPath(0, 0);
    }

    virtual bool nextBoolean(float pTrue) override final
    {
        return next1D() < pTrue;
    }
    virtual int nextDiscrete(int numChoices) override final
    {
        return min(int(next1D()*numChoices), numChoices - 1);
    }

    inline virtual float next1D() override final
    {
        int dimension = _vertexIdx*FullBlockSize + (_blockOffset++);
        SampleRecord &sample = _sampleVector[dimension];
        if (sample.time < _currentTime) {
            if (_largeStep) {
                push(dimension);
                if (!_frozen)
                    sample.value = _helperGenerator->next1D();
            } else {
                if (sample.time < _largeStepTime) {
                    sample.value = _helperGenerator->next1D();
                    sample.time = _largeStepTime;
                }
                for (int i = sample.time + 1; i < _currentTime; ++i)
                    sample.value = mutate(sample.value);
                sample.time = _currentTime - 1;
                push(dimension);
                if (!_frozen)
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

    virtual void seek(int vertex) override final
    {
        _vertexIdx = vertex;
        _blockOffset = 0;
    }

    virtual void putBoolean(float pTrue, bool choice) override final
    {
        put1D(choice ? untracked1D()*pTrue : pTrue + (1.0f - pTrue)*untracked1D());
    }

    virtual void putDiscrete(int numChoices, int choice) override final
    {
        put1D((choice + untracked1D())/numChoices);
    }

    virtual void put1D(float value) override final
    {
        if (std::isnan(value) || value > 1.0f) {
            value = 0.0f;
        }
        if (value == 1.0f)
            value = 0.0f;

        int dimension = _vertexIdx*FullBlockSize + (_blockOffset++);
        push(dimension);
        _sampleVector[dimension].value = value;
        if (_frozen)
            _sampleVector[dimension].time = _currentTime;
        else
            _sampleVector[dimension].time = _currentTime - 1;
    }

    virtual void put2D(Vec2f value) override final
    {
        put1D(value.x());
        put1D(value.y());
    }

    virtual float untracked1D() override final
    {
        return _helperGenerator->next1D();
    }

    virtual UniformSampler &uniformGenerator() override final
    {
        return *_helperGenerator;
    }
};

}

#endif /* WRITABLEMETROPOLISSAMPLER_HPP_ */
