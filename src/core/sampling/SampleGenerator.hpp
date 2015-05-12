#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

#include "io/FileUtils.hpp"

namespace Tungsten {

class SampleGenerator
{
public:
    virtual ~SampleGenerator() {}

    virtual void setup(uint32 pixelId, int sample) = 0;

    virtual void saveState(OutputStreamHandle &out) = 0;
    virtual void loadState(InputStreamHandle &in) = 0;

    virtual float next1D() = 0;
    virtual Vec2f next2D() = 0;
    virtual Vec3f next3D() = 0;
    virtual Vec4f next4D() = 0;
};

}

#endif /* SAMPLEGENERATOR_HPP_ */
