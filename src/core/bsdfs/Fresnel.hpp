#ifndef FRESNEL_HPP_
#define FRESNEL_HPP_

#include "math/MathUtil.hpp"

#include <cmath>

namespace Tungsten
{

namespace Fresnel
{

static inline float dielectricReflectance(float eta, float cosThetaI, float &cosThetaT)
{
    if (cosThetaI < 0.0f) {
        eta = 1.0f/eta;
        cosThetaI = -cosThetaI;
    }
    float sinThetaI = std::sqrt(max(1.0f - cosThetaI*cosThetaI, 0.0f));
    float sinThetaT = eta*sinThetaI;
    if (sinThetaT > 1.0f) {
        cosThetaT = 0.0f;
        return 1.0f;
    }
    cosThetaT = std::sqrt(max(1.0f - sinThetaT*sinThetaT, 0.0f));

    float Rs = (eta*cosThetaI - cosThetaT)/(eta*cosThetaI + cosThetaT);
    float Rp = (eta*cosThetaT - cosThetaI)/(eta*cosThetaT + cosThetaI);

    return (Rs*Rs + Rp*Rp)*0.5f;
}

static inline float dielectricReflectance(float eta, float cosThetaI)
{
    float cosThetaT;
    return dielectricReflectance(eta, cosThetaI, cosThetaT);
}

// From "PHYSICALLY BASED LIGHTING CALCULATIONS FOR COMPUTER GRAPHICS" by Peter Shirley
// http://www.cs.virginia.edu/~jdl/bib/globillum/shirley_thesis.pdf
static inline float conductorReflectance(float eta, float k, float cosThetaI)
{
    float cosThetaISq = cosThetaI*cosThetaI;
    float sinThetaISq = max(1.0f - cosThetaISq, 0.0f);
    float sinThetaIQu = sinThetaISq*sinThetaISq;

    float innerTerm = eta*eta - k*k - sinThetaISq;
    float aSqPlusBSq = std::sqrt(max(innerTerm*innerTerm + 4.0f*eta*eta*k*k, 0.0f));
    float a = std::sqrt(max((aSqPlusBSq + innerTerm)*0.5f, 0.0f));

    float Rs = ((aSqPlusBSq + cosThetaISq) - (2.0f*a*cosThetaI))/
               ((aSqPlusBSq + cosThetaISq) + (2.0f*a*cosThetaI));
    float Rp = ((cosThetaISq*aSqPlusBSq + sinThetaIQu) - (2.0f*a*cosThetaI*sinThetaISq))/
               ((cosThetaISq*aSqPlusBSq + sinThetaIQu) + (2.0f*a*cosThetaI*sinThetaISq));

    return 0.5f*(Rs + Rs*Rp);
}

static inline float conductorReflectanceApprox(float eta, float k, float cosThetaI)
{
    float cosThetaISq = cosThetaI*cosThetaI;
    float ekSq = eta*eta* + k*k;
    float cosThetaEta2 = cosThetaI*2.0f*eta;

    float Rp = (ekSq*cosThetaISq - cosThetaEta2 + 1.0f)/(ekSq*cosThetaISq + cosThetaEta2 + 1.0f);
    float Rs = (ekSq - cosThetaEta2 + cosThetaISq)/(ekSq + cosThetaEta2 + cosThetaISq);
    return (Rs + Rp)*0.5f;
}

static inline Vec3f conductorReflectance(const Vec3f &eta, const Vec3f &k, float cosThetaI)
{
    return Vec3f(
        conductorReflectance(eta.x(), k.x(), cosThetaI),
        conductorReflectance(eta.y(), k.y(), cosThetaI),
        conductorReflectance(eta.z(), k.z(), cosThetaI)
    );
}

}

}

#endif /* FRESNEL_HPP_ */
