#ifndef MULTIPLEXEDSTATS_HPP_
#define MULTIPLEXEDSTATS_HPP_

#include "ChainTracker.hpp"

namespace Tungsten {

class AtomicMultiplexedStats
{
    int _numBounces;
    AtomicChainTracker _techniqueChange;
    AtomicChainTracker _largeStep;
    AtomicChainTracker _smallStep;
    AtomicChainTracker _inversion;

public:
    AtomicMultiplexedStats(int numBounces)
    : _numBounces(numBounces),
      _techniqueChange(numBounces),
      _largeStep(numBounces),
      _smallStep(numBounces),
      _inversion(numBounces)
    {
    }

    AtomicChainTracker &techniqueChange()
    {
        return _techniqueChange;
    }

    AtomicChainTracker &largeStep()
    {
        return _largeStep;
    }

    AtomicChainTracker &smallStep()
    {
        return _smallStep;
    }

    AtomicChainTracker &inversion()
    {
        return _inversion;
    }

    int numBounces() const
    {
        return _numBounces;
    }
};

class MultiplexedStats
{
    ChainTracker _techniqueChange;
    ChainTracker _largeStep;
    ChainTracker _smallStep;
    ChainTracker _inversion;

public:
    MultiplexedStats(AtomicMultiplexedStats &parent)
    : _techniqueChange(parent.techniqueChange()),
      _largeStep(parent.largeStep()),
      _smallStep(parent.smallStep()),
      _inversion(parent.inversion())
    {
    }

    ChainTracker &techniqueChange()
    {
        return _techniqueChange;
    }

    ChainTracker &largeStep()
    {
        return _largeStep;
    }

    ChainTracker &smallStep()
    {
        return _smallStep;
    }

    ChainTracker &inversion()
    {
        return _inversion;
    }
};

}

#endif /* MULTIPLEXEDSTATS_HPP_ */
