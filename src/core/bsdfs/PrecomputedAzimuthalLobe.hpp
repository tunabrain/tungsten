#ifndef PRECOMPUTEDAZIMUTHALLOBE_HPP_
#define PRECOMPUTEDAZIMUTHALLOBE_HPP_

#include "sampling/InterpolatedDistribution1D.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include <vector>
#include <memory>

namespace Tungsten {

// Utility class used by the RoughDielectricBcsdf for precomputing
// the azimuthal scattering functions
class PrecomputedAzimuthalLobe
{
public:
    static CONSTEXPR int AzimuthalResolution = 64;

private:
    std::unique_ptr<Vec3f[]> _table;
    std::unique_ptr<InterpolatedDistribution1D> _sampler;

public:
    PrecomputedAzimuthalLobe(std::unique_ptr<Vec3f[]> table);

    void sample(float cosThetaD, float xi, float &phi, float &pdf) const
    {
        float v = (AzimuthalResolution - 1)*cosThetaD;

        int x;
        _sampler->warp(v, xi, x);

        phi = TWO_PI*(x + xi)*(1.0f/AzimuthalResolution);
        pdf = _sampler->pdf(v, x)*float(AzimuthalResolution*INV_TWO_PI);
    }

    Vec3f eval(float phi, float cosThetaD) const
    {
        float u = (AzimuthalResolution - 1)*phi*INV_TWO_PI;
        float v = (AzimuthalResolution - 1)*cosThetaD;
        int x0 = clamp(int(u), 0, AzimuthalResolution - 2);
        int y0 = clamp(int(v), 0, AzimuthalResolution - 2);
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        u = clamp(u - x0, 0.0f, 1.0f);
        v = clamp(v - y0, 0.0f, 1.0f);

        return (_table[x0 + y0*AzimuthalResolution]*(1.0f - u) + _table[x1 + y0*AzimuthalResolution]*u)*(1.0f - v) +
               (_table[x0 + y1*AzimuthalResolution]*(1.0f - u) + _table[x1 + y1*AzimuthalResolution]*u)*v;
    }

    float pdf(float phi, float cosThetaD) const
    {
        float u = (AzimuthalResolution - 1)*phi*INV_TWO_PI;
        float v = (AzimuthalResolution - 1)*cosThetaD;
        return _sampler->pdf(v, int(u))*float(AzimuthalResolution*INV_TWO_PI);
    }

    float weight(float cosThetaD) const
    {
        float v = (AzimuthalResolution - 1)*cosThetaD;
        return _sampler->sum(v)*(TWO_PI/AzimuthalResolution);
    }
};

}



#endif /* PRECOMPUTEDAZIMUTHALLOBE_HPP_ */
