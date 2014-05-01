#ifndef PHASEFUNCTION_HPP_
#define PHASEFUNCTION_HPP_

#include "sampling/Sample.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "Debug.hpp"

#include <string>
#include <cmath>

namespace Tungsten
{

class PhaseFunction
{
public:
    enum Type
    {
        Isotropic,
        HenyeyGreenstein,
        Rayleigh,
    };

    static Type stringToType(const std::string &s)
    {
        if (s == "isotropic")
            return Isotropic;
        else if (s == "henyey_greenstein")
            return HenyeyGreenstein;
        else if (s == "rayleigh")
            return Rayleigh;
        FAIL("Invalid phase function: '%s'", s.c_str());
        return Isotropic;
    }

    static float eval(Type type, float cosTheta, float g)
    {
        switch (type) {
        case Isotropic:
            return INV_FOUR_PI;
        case HenyeyGreenstein:
            return INV_FOUR_PI*(1.0f - g*g)/std::pow(1.0f + g*g - 2.0f*g*cosTheta, 1.5f);
        case Rayleigh:
            return (3.0f/(16.0f*PI))*(1.0f + cosTheta*cosTheta);
        }
        return 0.0f;
    }

    static Vec3f sample(Type type, float g, const Vec2f &sample)
    {
        float phi = sample.x()*TWO_PI;
        float cosTheta = 0.0f;
        if (g == 0.0f)
            type = Isotropic;
        switch (type) {
        case Isotropic:
            cosTheta = sample.y()*2.0f - 1.0f;
            break;
        case HenyeyGreenstein:
            cosTheta = (1.0f + g*g - sqr((1.0f - g*g)/(1.0f + g*(sample.y()*2.0f - 1.0f))))/(2.0f*g);
            break;
        case Rayleigh: {
            float z = sample.y()*4.0f - 2.0f;
            float invZ = std::sqrt(z*z + 1.0f);
            cosTheta = std::pow(z + invZ, 1.0f/3.0f) + std::pow(z - invZ, 1.0f/3.0f);
        }}
        float sinTheta = std::sqrt(max(1.0f - cosTheta*cosTheta, 0.0f));
        return Vec3f(
            std::cos(phi)*sinTheta,
            std::sin(phi)*sinTheta,
            cosTheta
        );
    }
};

}

#endif /* PHASEFUNCTION_HPP_ */
