#ifndef ANGLE_HPP_
#define ANGLE_HPP_

namespace Tungsten
{

constexpr float PI          = 3.1415926536f;
constexpr float PI_HALF     = PI*0.5f;
constexpr float TWO_PI      = PI*2.0f;
constexpr float INV_PI      = 1.0f/PI;
constexpr float INV_TWO_PI  = 0.5f*INV_PI;
constexpr float INV_FOUR_PI = 0.25f*INV_PI;

class Angle
{
public:
    static constexpr float radToDeg(float a)
    {
        return a*(180.0f/PI);
    }

    static constexpr float degToRad(float a)
    {
        return a*(PI/180.0f);
    }

    static constexpr float angleRepeat(float a)
    {
        return std::fmod(std::fmod(a, TWO_PI) + TWO_PI, TWO_PI);
    }
};

}

#endif /* ANGLE_HPP_ */
