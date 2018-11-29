#include "DavisWeinsteinTransmittance.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

DavisWeinsteinTransmittance::DavisWeinsteinTransmittance()
: _h(0.75f),
  _c(1.0f)
{
}

void DavisWeinsteinTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    value.getField("h", _h);
    value.getField("c", _c);
    if (_h < 0.5f || _h > 1.0f)
        std::cout << "Warning: Valid range of Davis Weinstein transmittance is [0.5, 1.0]. Clamping current value (" << _h << ") to within range" << std::endl;
    _h = clamp(_h, 0.5f, 1.0f);
}

rapidjson::Value DavisWeinsteinTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "davis",
        "h", _h,
        "c", _c
    };
}

float DavisWeinsteinTransmittance::computeAlpha(float tau) const
{
    float beta = 2.0f*_h - 1.0f;
    return std::pow(tau, 1 - beta)/(std::pow(_c, 1 + beta));
}

Vec3f DavisWeinsteinTransmittance::surfaceSurface(const Vec3f &tau) const
{
    float alpha = computeAlpha(tau[0]);
    float Tr = std::pow(1.0f + tau[0]/alpha, -alpha);

    return Vec3f(std::isnan(Tr) ? 0 : Tr);
}
Vec3f DavisWeinsteinTransmittance::surfaceMedium(const Vec3f &tau) const
{
    float beta = 2.0f*_h - 1.0f;
    float t = tau[0];
    float alpha = computeAlpha(t);
    float base = 1.0f + t/alpha;
    float trSurface = std::pow(base, -alpha);

    float Tr = trSurface*(beta/base - (beta - 1.0f)*alpha/t*std::log(base));

    return Vec3f(std::isnan(Tr) ? 0 : Tr);
}
Vec3f DavisWeinsteinTransmittance::mediumSurface(const Vec3f &tau) const
{
    return surfaceMedium(tau);
}
Vec3f DavisWeinsteinTransmittance::mediumMedium(const Vec3f &tau) const
{
    float beta = 2.0f*_h - 1.0f;
    float t = tau[0];
    float alpha = computeAlpha(t);
    float base = 1.0f + t/alpha;
    float logBase = std::log(base);
    float trSurface = std::pow(base, -alpha);
    float term1 = beta*(-1.0f + beta*(1.0f + t) + (-1.0f + 2.0f*beta)*t/alpha)/(t*base*base);
    float term2 = ((-1.0f + beta)*beta*alpha/(t*t)*(2.0f*t + base)*logBase)/base;
    float term3 = (beta - 1.0f)*alpha/t*logBase;

    float Tr = trSurface*(term1 - term2 + term3*term3);

    return Vec3f(std::isnan(Tr) ? 0 : Tr);
}

float DavisWeinsteinTransmittance::sigmaBar() const
{
    return 1.0f;
}

// We have no real way of analytically sampling this function, so a simple bisection will have to do
float DavisWeinsteinTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    float xi = sampler.next1D();
    auto cdf = [this](float tau) { return 1.0f - surfaceSurface(Vec3f(tau))[0]; };
    float step = 1e6;
    float result = step*2;
    while (step > 1e-6) {
        if (cdf(result) > xi)
            result -= step;
        else
            result += step;
        step /= 2;
    }

    return result;
}
float DavisWeinsteinTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    float xi = sampler.next1D();
    auto cdf = [this](float tau) { return 1.0f - mediumSurface(Vec3f(tau))[0]; };
    float step = 1e6;
    float result = step * 2;
    while (step > 1e-6) {
        if (cdf(result) > xi)
            result -= step;
        else
            result += step;
        step /= 2;
    }

    return result;
}

}
