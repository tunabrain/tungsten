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

public:
    AtomicMultiplexedStats(int numBounces)
    : _numBounces(numBounces),
      _techniqueChange(numBounces),
      _largeStep(numBounces),
      _smallStep(numBounces)
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

public:
    MultiplexedStats(AtomicMultiplexedStats &parent)
    : _techniqueChange(parent.techniqueChange()),
      _largeStep(parent.largeStep()),
      _smallStep(parent.smallStep())
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
};

}

#endif /* MULTIPLEXEDSTATS_HPP_ */
