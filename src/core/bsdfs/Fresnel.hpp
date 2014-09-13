#ifndef FRESNEL_HPP_
#define FRESNEL_HPP_

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include <cmath>

namespace Tungsten {

namespace Fresnel {

// Computes total reflectance from an infinitesimally thin film with refraction index eta
// from all internal reflection/refraction events
static inline float thinFilmReflectance(float eta, float cosThetaI, float &cosThetaT)
{
    float sinThetaTSq = eta*eta*(1.0f - cosThetaI*cosThetaI);
    if (sinThetaTSq > 1.0f) {
        cosThetaT = 0.0f;
        return 1.0f;
    }
    cosThetaT = std::sqrt(max(1.0f - sinThetaTSq, 0.0f));

    float Rs = sqr((eta*cosThetaI - cosThetaT)/(eta*cosThetaI + cosThetaT));
    float Rp = sqr((eta*cosThetaT - cosThetaI)/(eta*cosThetaT + cosThetaI));

    return 1.0f - ((1.0f - Rs)/(1.0f + Rs) + (1.0f - Rp)/(1.0f + Rp))*0.5f;
}

static inline float thinFilmReflectance(float eta, float cosThetaI)
{
    float cosThetaT;
    return thinFilmReflectance(eta, cosThetaI, cosThetaT);
}

// Computes total reflectance including spectral interference from a thin film
// that is `thickness' nanometers thick and has refraction index eta
// See http://www.gamedev.net/page/resources/_/technical/graphics-programming-and-theory/thin-film-interference-for-computer-graphics-r2962
static inline Vec3f thinFilmReflectanceInterference(float eta, float cosThetaI, float thickness, float &cosThetaT)
{
    const Vec3f invLambdas = 1.0f/Vec3f(650.0f, 510.0f, 475.0f);

    float cosThetaISq = cosThetaI*cosThetaI;
    float sinThetaISq = 1.0f - cosThetaISq;
    float invEta = 1.0f/eta;

    float sinThetaTSq = eta*eta*sinThetaISq;
    if (sinThetaTSq > 1.0f) {
        cosThetaT = 0.0f;
        return Vec3f(1.0f);
    }
    cosThetaT = std::sqrt(1.0f - sinThetaTSq);

    float Ts = 4.0f*eta*cosThetaI*cosThetaT/sqr(eta*cosThetaI + cosThetaT);
    float Tp = 4.0f*eta*cosThetaI*cosThetaT/sqr(eta*cosThetaT + cosThetaI);

    float Rs = 1.0f - Ts;
    float Rp = 1.0f - Tp;

    Vec3f phi = (thickness*cosThetaT*FOUR_PI*invEta)*invLambdas;
    Vec3f cosPhi(std::cos(phi.x()), std::cos(phi.y()), std::cos(phi.z()));

    Vec3f tS = sqr(Ts)/((sqr(Rs) + 1.0f) - 2.0f*Rs*cosPhi);
    Vec3f tP = sqr(Tp)/((sqr(Rp) + 1.0f) - 2.0f*Rp*cosPhi);

    return 1.0f - (tS + tP)*0.5f;
}

static inline Vec3f thinFilmReflectanceInterference(float eta, float cosThetaI, float thickness)
{
    float cosThetaT;
    return thinFilmReflectanceInterference(eta, cosThetaI, thickness, cosThetaT);
}

static inline float dielectricReflectance(float eta, float cosThetaI, float &cosThetaT)
{
    if (cosThetaI < 0.0f) {
        eta = 1.0f/eta;
        cosThetaI = -cosThetaI;
    }
    float sinThetaTSq = eta*eta*(1.0f - cosThetaI*cosThetaI);
    if (sinThetaTSq > 1.0f) {
        cosThetaT = 0.0f;
        return 1.0f;
    }
    cosThetaT = std::sqrt(max(1.0f - sinThetaTSq, 0.0f));

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

// Computes hemispherical integral of dielectricReflectance(ior, cos(theta))*cos(theta)
static inline float computeDiffuseFresnel(float ior, const int sampleCount)
{
    double diffuseFresnel = 0.0;
    float fb = Fresnel::dielectricReflectance(ior, 0.0f);
    for (int i = 1; i <= sampleCount; ++i) {
        float cosThetaSq = float(i)/sampleCount;
        float fa = Fresnel::dielectricReflectance(ior, min(std::sqrt(cosThetaSq), 1.0f));
        diffuseFresnel += double(fa + fb)*(0.5/sampleCount);
        fb = fa;
    }

    return float(diffuseFresnel);
}

}

}

#endif /* FRESNEL_HPP_ */
