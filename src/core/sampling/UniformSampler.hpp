#ifndef UNIFORMSAMPLER_HPP_
#define UNIFORMSAMPLER_HPP_

#include "math/BitManip.hpp"
#include "math/Vec.hpp"

#include "io/FileUtils.hpp"

namespace Tungsten {

class UniformSampler
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

    void saveState(OutputStreamHandle &out)
    {
        FileUtils::streamWrite(out, _state);
    }

    void loadState(InputStreamHandle &in)
    {
        FileUtils::streamRead(in, _state);
    }

    // PCG random number generator
    // See http://www.pcg-random.org/
    inline uint32 nextI()
    {
        uint64 oldState = _state;
        _state = oldState*6364136223846793005ULL + (_sequence | 1);
        uint32 xorShifted = uint32(((oldState >> 18u) ^ oldState) >> 27u);
        uint32 rot = oldState >> 59u;
        return (xorShifted >> rot) | (xorShifted << (uint32(-int32(rot)) & 31));
    }

    inline float next1D()
    {
        return BitManip::normalizedUint(nextI());
    }

    inline Vec2f next2D()
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
