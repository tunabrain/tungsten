#ifndef SAMPLEGENERATOR_HPP_
#define SAMPLEGENERATOR_HPP_

#include "math/BitManip.hpp"
#include "math/MathUtil.hpp"

namespace Tungsten {

class SampleGenerator
{
public:
    virtual ~SampleGenerator() {}

    virtual void setup(uint32 pixelId, int sample) = 0;

    virtual uint32 nextI() = 0;

    virtual float next1D()
    {
        return BitManip::normalizedUint(nextI());
    }

    virtual Vec2f next2D()
    {
        float a = next1D();
        float b = next1D();
        return Vec2f(a, b);
    }

    virtual Vec3f next3D()
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        return Vec3f(a, b, c);
    }

    virtual Vec4f next4D()
    {
        float a = next1D();
        float b = next1D();
        float c = next1D();
        float d = next1D();
        return Vec4f(a, b, c, d);
    }
};

}

#endif /* SAMPLEGENERATOR_HPP_ */
