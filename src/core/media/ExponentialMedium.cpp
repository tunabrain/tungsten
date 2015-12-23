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

void ExponentialMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "sigma_a", _materialSigmaA);
    JsonUtils::fromJson(v, "sigma_s", _materialSigmaS);
    JsonUtils::fromJson(v, "density", _density);
    JsonUtils::fromJson(v, "falloff_scale", _falloffScale);
    JsonUtils::fromJson(v, "unit_point", _unitPoint);
    JsonUtils::fromJson(v, "falloff_direction", _falloffDirection);
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

inline float ExponentialMedium::inverseOpticalDepth(float x, float dx, float sigmaT, float logXi) const
{
    if (dx == 0.0f) {
        float effectiveSigmaTc = sigmaT*std::exp(-x);
        return -logXi/effectiveSigmaTc;
    } else {
        float denom = sigmaT + dx*std::exp(x)*logXi;
        return denom <= 0.0f ? Ray::infinity() : std::log(sigmaT/denom)/dx;
    }
}

bool ExponentialMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    if (state.bounce > _maxBounce)
        return false;

    float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
    float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

    float maxT = ray.farT();
    if (_absorptionOnly) {
        if (maxT == Ray::infinity() && dx <= 0.0f)
            return false;
        sample.t = maxT;
        sample.weight = std::exp(-_sigmaT*densityIntegral(x, dx, ray.farT()));
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float xi = 1.0f - sampler.next1D();
        float logXi = std::log(xi);

        float t = inverseOpticalDepth(x, dx, sigmaTc, logXi);
        sample.t = min(t, maxT);
        sample.weight = std::exp(-_sigmaT*densityIntegral(x, dx, sample.t));
        sample.exited = (t >= maxT);
        if (sample.exited) {
            sample.pdf = sample.weight.avg();
        } else {
            float rho = density(x, dx, sample.t);
            sample.pdf = (rho*_sigmaT*sample.weight).avg();
            sample.weight *= rho*_sigmaS;
        }
        sample.weight /= sample.pdf;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}
Vec3f ExponentialMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray) const
{
    float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
    float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

    if (ray.farT() == Ray::infinity() && dx <= 0.0f)
        return Vec3f(0.0f);
    else
        return std::exp(-_sigmaT*densityIntegral(x, dx, ray.farT()));
}

float ExponentialMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool onSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
        float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

        Vec3f transmittance = std::exp(-_sigmaT*densityIntegral(x, dx, ray.farT()));
        if (onSurface) {
            return transmittance.avg();
        } else {
            return (density(x, dx, ray.farT())*_sigmaT*transmittance).avg();
        }
    }
}

Vec3f ExponentialMedium::transmittanceAndPdfs(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    float  x = _falloffScale*(ray.pos() - _unitPoint).dot(_unitFalloffDirection);
    float dx = _falloffScale*ray.dir().dot(_unitFalloffDirection);

    if (ray.farT() == Ray::infinity() && dx <= 0.0f) {
        pdfForward = pdfBackward = 0.0f;
        return Vec3f(0.0f);
    }

    Vec3f transmittance = std::exp(-_sigmaT*densityIntegral(x, dx, ray.farT()));

    if (_absorptionOnly) {
        pdfForward = pdfBackward = 1.0f;
    } else {
        pdfForward  =   endOnSurface ? transmittance.avg() : (density(x, dx, ray.farT())*_sigmaT*transmittance).avg();
        pdfBackward = startOnSurface ? transmittance.avg() : (density(x, dx,       0.0f)*_sigmaT*transmittance).avg();
    }

    return transmittance;
}

}
