#ifndef UNIFORMSAMPLER_HPP_
#define UNIFORMSAMPLER_HPP_

#include "SampleGenerator.hpp"

namespace Tungsten {

class UniformSampler : public SampleGenerator
{
    uint64 _state;
    uint64 _sequence;

public:
    UniformSampler()
    : UniformSampler(0xBA5EBA11)
    {
    }

    UniformSampler(uint64 seed, uint64 sequence = 0)
    : _state(seed),
      _sequence(sequence)
    {
    }

    virtual void saveState(OutputStreamHandle &out) override final
    {
        FileUtils::streamWrite(out, _state);
    }

    virtual void loadState(InputStreamHandle &in)  override final
    {
        FileUtils::streamRead(in, _state);
    }

    virtual void setup(uint32 /*pixelId*/, int /*sample*/) override final
    {
    }

    // PCG random number generator
    // See http://www.pcg-random.org/
    inline uint32 nextI()
    {
        uint64 oldState = _state;
        _state = oldState*6364136223846793005ULL + (_sequence | 1);
        uint32 xorShifted = ((oldState >> 18u) ^ oldState) >> 27u;
        uint32 rot = oldState >> 59u;
        return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
    }

    inline virtual float next1D() override final
    {
        return BitManip::normalizedUint(nextI());
    }

    inline virtual Vec2f next2D() override final
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    uint64 state() const
    {
        return _state;
    }

    uint64 sequence() const
    {
        return _sequence;
    }
};

}

#endif /* UNIFORMSAMPLER_HPP_ */
