#ifndef PATHSAMPLEGENERATOR_HPP_
#define PATHSAMPLEGENERATOR_HPP_

#include "math/Vec.hpp"

#include "io/FileUtils.hpp"

namespace Tungsten {

enum SampleBlockStructure
{
    NumContinuousSamples = 6,
    NumTransmittanceSamples = 1,
    NumDiscreteSamples = 4,

    ContinuousBlockOffset = 0,
    TransmittanceBlockOffset = ContinuousBlockOffset + NumContinuousSamples,
    DiscreteBlockOffset = TransmittanceBlockOffset + NumTransmittanceSamples,

    FullBlockSize = NumContinuousSamples + NumDiscreteSamples + NumTransmittanceSamples,
    ContinuousBlockSize = NumContinuousSamples + NumTransmittanceSamples
};

enum SampleBlock : int
{
    EmitterSample               = ContinuousBlockOffset,
    CameraSample                = ContinuousBlockOffset,
    BsdfSample                  = ContinuousBlockOffset,
    MediumPhaseSample           = ContinuousBlockOffset,
    MediumTransmittanceSample   = TransmittanceBlockOffset,
    DiscreteEmitterSample       = DiscreteBlockOffset,
    DiscreteCameraSample        = DiscreteBlockOffset,
    DiscreteBsdfSample          = DiscreteBlockOffset,
    DiscreteMediumSample        = DiscreteBlockOffset,
    DiscreteTransmittanceSample = DiscreteBlockOffset + 1,
    DiscreteTransparencySample  = DiscreteBlockOffset + 2,
    DiscreteRouletteSample      = DiscreteBlockOffset + 3,
};

class PathSampleGenerator
{
public:
    virtual ~PathSampleGenerator() {}

    virtual void startPath(uint32 pixelId, int sample) = 0;
    virtual void advancePath() = 0;
    // FIXME: advancePath() is currently never called for direct camera
    // connections. This is because there is currently no use case where
    // random numbers are consumed during this process, but there are going
    // to be in the future. This requires some re-evaluation of random
    // numbers for direct samples

    virtual void saveState(OutputStreamHandle &out) = 0;
    virtual void loadState(InputStreamHandle &in) = 0;

    virtual bool nextBoolean(SampleBlock block, float pTrue) = 0;
    virtual int nextDiscrete(SampleBlock block, int numChoices) = 0;
    virtual float next1D(SampleBlock block) = 0;
    virtual Vec2f next2D(SampleBlock block) = 0;
};

}

#endif /* PATHSAMPLEGENERATOR_HPP_ */
