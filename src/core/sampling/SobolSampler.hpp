#ifndef SOBOLSAMPLER_HPP_
#define SOBOLSAMPLER_HPP_

#include "SampleGenerator.hpp"

#include "Debug.hpp"

#include <sobol/sobol.h>

namespace Tungsten {

class SobolSampler : public SampleGenerator
{
    uint32 _scramble;
    uint32 _index;
    int _dimension;

    inline uint32 permutedIndex() const
    {
        return (_index & ~0xFF) | ((_index + _scramble) & 0xFF);
    }

public:
    SobolSampler()
    : _scramble(0), _index(0), _dimension(0)
    {
    }

    virtual void saveState(OutputStreamHandle &/*out*/) override final
    {
    }

    virtual void loadState(InputStreamHandle &/*in*/)  override final
    {
    }

    virtual void setup(uint32 pixelId, int sample) override final
    {
        _scramble = MathUtil::hash32(pixelId);
        _index = sample;
        _dimension = 0;
    }

    virtual uint32 nextI() override final
    {
        if (_dimension >= 1024)
            FAIL("Sobol sampler dimension > 1024!");
        return sobol::sample(permutedIndex(), _dimension++, _scramble);
    }
};

}



#endif /* SOBOLSAMPLER_HPP_ */
