#ifndef TRANSMITTANCE_HPP_
#define TRANSMITTANCE_HPP_

#include "sampling/PathSampleGenerator.hpp"

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

#include "StringableEnum.hpp"

#include <algorithm>
#include <array>

namespace Tungsten {

class Transmittance : public JsonSerializable
{
public:
    virtual ~Transmittance() {}

    inline Vec3f eval(const Vec3f &tau, bool startOnSurface, bool endOnSurface) const
    {
        if (startOnSurface && endOnSurface)
            return surfaceSurface(tau);
        else if (!startOnSurface && !endOnSurface)
            return mediumMedium(tau)/sigmaBar();
        else
            return mediumSurface(tau);
    }
    inline float sample(PathSampleGenerator &sampler, bool startOnSurface) const
    {
        return startOnSurface ? sampleSurface(sampler) : sampleMedium(sampler);
    }
    inline Vec3f surfaceProbability(const Vec3f &tau, bool startOnSurface) const
    {
        return startOnSurface ? surfaceSurface(tau) : mediumSurface(tau);
    }
    inline Vec3f mediumPdf(const Vec3f &tau, bool startOnSurface) const
    {
        return startOnSurface ? surfaceMedium(tau) : mediumMedium(tau);
    }

    virtual bool isDirac() const // Returns true if mediumMedium is a dirac/sum of dirac deltas
    {
        return false;
    }

    virtual Vec3f surfaceSurface(const Vec3f &tau) const = 0;
    virtual Vec3f surfaceMedium(const Vec3f &tau) const = 0;
    virtual Vec3f mediumSurface(const Vec3f &tau) const = 0;
    virtual Vec3f mediumMedium(const Vec3f &tau) const = 0;

    virtual float sigmaBar() const = 0; // Returns surfaceMedium(x)/mediumSurface(x) (i.e. identical to surfaceMedium(0))

    virtual float sampleSurface(PathSampleGenerator &sampler) const = 0;
    virtual float sampleMedium(PathSampleGenerator &sampler) const = 0;
};

}

#endif /* TRANSMITTANCE_HPP_ */
