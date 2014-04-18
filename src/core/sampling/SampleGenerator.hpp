#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "extern/sobol/sobol.h"

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

#include "Config.hpp"

namespace Tungsten
{

class SobolSampler
{
    uint32 _seed;
    uint64 _index;

public:
    SobolSampler(uint32 seed, uint32 sample)
    : _seed(MathUtil::hash32(seed)), _index(sample)
    {
    }

    uint32 iSample(unsigned dim)
    {
        return sobol::sample(_index + _seed, dim, _seed) + _seed;
    }

    float fSample(unsigned dim)
    {
        return BitManip::normalizedUint(iSample(dim));
    }
};

class UniformSampler
{
    uint32 _seed;

public:
    UniformSampler(uint32 seed, uint32 /*sample*/)
    : _seed(seed)
    {
    }

    uint32 iSample(unsigned /*dim*/)
    {
        _seed = (_seed*1103515245u + 12345u) & 0x7FFFFFFF;
        return _seed;
    }

    float fSample(unsigned dim)
    {
        return BitManip::normalizedUint(iSample(dim) << 1);
    }

    uint32 seed() const
    {
        return _seed;
    }
};

#if USE_SOBOL
typedef SobolSampler SampleGenerator;
#else
typedef UniformSampler SampleGenerator;
#endif

}

#endif /* SAMPLEGENERATOR_HPP_ */
