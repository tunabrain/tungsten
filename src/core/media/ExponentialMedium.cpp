#include "ExponentialMedium.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/TangentFrame.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

ExponentialMedium::ExponentialMedium()
: _materialSigmaA(0.0f),
  _materialSigmaS(0.0f),
  _density(1.0f),
  _falloffScale(1.0f),
  _unitPoint(0.0f),
  _falloffDirection(0.0f, 1.0f, 0.0f)
{
}

void ExponentialMedium::fromJson(JsonPtr value, const Scene &scene)
{
    Medium::fromJson(value, scene);
    value.getField("sigma_a", _materialSigmaA);
    value.getField("sigma_s", _materialSigmaS);
    value.getField("density", _density);
    value.getField("falloff_scale", _falloffScale);
    value.getField("unit_point", _unitPoint);
    value.getField("falloff_direction", _falloffDirection);
}

rapidjson::Value ExponentialMedium::toJson(Allocator &allocator) const
{
    return JsonObject{Medium::toJson(allocator), allocator,
        "type", "exponential",
        "sigma_a", _materialSigmaA,
        "sigma_s", _materialSigmaS,
        "density", _density,
        "falloff_scale", _falloffScale,
        "unit_point", _unitPoint,
        "falloff_direction", _falloffDirection
    };
}

bool ExponentialMedium::isHomogeneous() const
{
    return false;
}

void ExponentialMedium::prepareForRender()
{
    _unitFalloffDirection = _falloffDirection.normalized();
    _sigmaA = _materialSigmaA*_density;
    _sigmaS = _materialSigmaS*_density;
    _sigmaT = _sigmaA + _sigmaS;
    _absorptionOnly = _sigmaS == 0.0f;
}

Vec3f ExponentialMedium::sigmaA(Vec3f p) const
{
    return density(p)*_sigmaA;
}

Vec3f ExponentialMedium::sigmaS(Vec3f p) const
{
    return density(p)*_sigmaS;
}

Vec3f ExponentialMedium::sigmaT(Vec3f p) const
{
    return density(p)*_sigmaT;
}

inline float ExponentialMedium::density(Vec3f p) const
{
    return std::exp(-_falloffScale*(p - _unitPoint).dot(_unitFalloffDirection));
}

inline float ExponentialMedium::density(float x, float dx, float t) const
{
    return std::exp(-(x + dx*t));
}

inline float ExponentialMedium::densityIntegral(float x, float dx, float tMax) const
{
    if (tMax == Ray::infinity())
        return std::exp(-x)/dx;
    else if (dx == 0.0f)
        return std::exp(-x)*tMax;
    else
        return (std::exp(-x) - std::exp(-dx*tMax - x))/dx;
}

inline float ExponentialMedium::inverseOpticalDepth(float x, float dx, float tau) const
{
    if (dx == 0.0f) {
        return tau/std::exp(-x);
    } else {
        float denom = 1.0f - dx*std::exp(x)*tau;
        return denom <= 0.0f ? Ray::infinity() : -std::log(denom)/dx;
    }
}

bool ExponentialMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    sample.emission = Vec3f(0.0f);

    if (state.bounce > _maxBounce)
        return false;

    float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
    float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

    float maxT = ray.farT();
    if (_absorptionOnly) {
        if (maxT == Ray::infinity() && dx <= 0.0f)
            return false;
        sample.t = maxT;
        Vec3f tau = densityIntegral(x, dx, ray.farT())*_sigmaT;
        sample.weight = _transmittance->eval(tau, state.firstScatter, true);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float tauC = _transmittance->sample(sampler, state.firstScatter)/sigmaTc;

        float t = inverseOpticalDepth(x, dx, tauC);
        sample.t = min(t, maxT);
        Vec3f tau = densityIntegral(x, dx, sample.t)*_sigmaT;
        sample.weight = _transmittance->eval(tau, state.firstScatter, sample.exited);
        sample.exited = (t >= maxT);
        if (sample.exited) {
            sample.pdf = _transmittance->surfaceProbability(tau, state.firstScatter).avg();
        } else {
            float rho = density(x, dx, sample.t);
            sample.pdf = (rho*_sigmaT*_transmittance->mediumPdf(tau, state.firstScatter)).avg();
            sample.weight *= rho*_sigmaS*_transmittance->sigmaBar();
        }
        sample.weight /= sample.pdf;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}
Vec3f ExponentialMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface) const
{
    float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
    float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

    if (ray.farT() == Ray::infinity() && dx <= 0.0f) {
        return Vec3f(0.0f);
    } else {
        Vec3f tau = _sigmaT*densityIntegral(x, dx, ray.farT());
        return _transmittance->eval(tau, startOnSurface, endOnSurface);
    }
}

float ExponentialMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface, bool endOnSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
        float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

        Vec3f tau = _sigmaT*densityIntegral(x, dx, ray.farT());
        if (endOnSurface)
            return _transmittance->surfaceProbability(tau, startOnSurface).avg();
        else
            return (density(x, dx, ray.farT())*_sigmaT*_transmittance->mediumPdf(tau, startOnSurface)).avg();
    }
}

}
