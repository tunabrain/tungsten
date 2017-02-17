#ifndef PATHSAMPLEGENERATOR_HPP_
#define PATHSAMPLEGENERATOR_HPP_

#include "UniformSampler.hpp"

#include "math/Vec.hpp"

#include "io/FileUtils.hpp"

namespace Tungsten {

class PathSampleGenerator
{
public:
    virtual ~PathSampleGenerator() {}

    virtual void startPath(uint32 pixelId, uint32 sample) = 0;
    virtual void advancePath() = 0;

    virtual void saveState(OutputStreamHandle &out) = 0;
    virtual void loadState(InputStreamHandle &in) = 0;

    virtual bool nextBoolean(float pTrue) = 0;
    virtual int nextDiscrete(int numChoices) = 0;
    virtual float next1D() = 0;
    virtual Vec2f next2D() = 0;

    virtual UniformSampler &uniformGenerator() = 0;
};

}

#endif /* PATHSAMPLEGENERATOR_HPP_ */
