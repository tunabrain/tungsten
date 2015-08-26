#ifndef ANGLE_HPP_
#define ANGLE_HPP_

#include <cmath>

namespace Tungsten {

CONSTEXPR float PI          = 3.1415926536f;
CONSTEXPR float PI_HALF     = PI*0.5f;
CONSTEXPR float TWO_PI      = PI*2.0f;
CONSTEXPR float FOUR_PI     = PI*4.0f;
CONSTEXPR float INV_PI      = 1.0f/PI;
CONSTEXPR float INV_TWO_PI  = 0.5f*INV_PI;
CONSTEXPR float INV_FOUR_PI = 0.25f*INV_PI;
CONSTEXPR float SQRT_PI     = 1.77245385091f;
CONSTEXPR float INV_SQRT_PI = 1.0f/SQRT_PI;

class Angle
{
public:
    static CONSTEXPR float radToDeg(float a)
    {
        return a*(180.0f/PI);
    }

    static CONSTEXPR float degToRad(float a)
    {
        return a*(PI/180.0f);
    }

    static float angleRepeat(float a)
    {
        return std::fmod(std::fmod(a, TWO_PI) + TWO_PI, TWO_PI);
    }
};

}

#endif /* ANGLE_HPP_ */
