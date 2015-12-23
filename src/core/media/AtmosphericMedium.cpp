#include "AtmosphericMedium.hpp"

#include "primitives/Primitive.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Erf.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

AtmosphericMedium::AtmosphericMedium()
: _scene(nullptr),
  _materialSigmaA(0.0f),
  _materialSigmaS(0.0f),
  _density(1.0f),
  _falloffScale(1.0f),
  _radius(1.0f),
  _center(0.0f)
{
}

void AtmosphericMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    _scene = &scene;

    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "pivot", _primName);
    JsonUtils::fromJson(v, "sigma_a", _materialSigmaA);
    JsonUtils::fromJson(v, "sigma_s", _materialSigmaS);
    JsonUtils::fromJson(v, "density", _density);
    JsonUtils::fromJson(v, "falloff_scale", _falloffScale);
    JsonUtils::fromJson(v, "radius", _radius);
    JsonUtils::fromJson(v, "center", _center);
}

rapidjson::Value AtmosphericMedium::toJson(Allocator &allocator) const
{
    JsonObject result{Medium::toJson(allocator), allocator,
        "type", "atmosphere",
        "sigma_a", _materialSigmaA,
        "sigma_s", _materialSigmaS,
        "density", _density,
        "falloff_scale", _falloffScale,
        "radius", _radius
    };
    if (!_primName.empty())
        result.add("pivot", _primName);
    else
        result.add("center", _center);

    return result;
}

bool AtmosphericMedium::isHomogeneous() const
{
    return false;
}

void AtmosphericMedium::prepareForRender()
{
    if (!_primName.empty()) {
        const Primitive *prim = _scene->findPrimitive(_primName);
        if (!prim)
            DBG("Note: unable to find pivot object '%s' for atmospheric medium", _primName.c_str());
        else
            _center = prim->transform()*Vec3f(0.0f);
    }

    _effectiveFalloffScale = _falloffScale/_radius;
    _sigmaA = _materialSigmaA*_density;
    _sigmaS = _materialSigmaS*_density;
    _sigmaT = _sigmaA + _sigmaS;
    _absorptionOnly = _sigmaS == 0.0f;
}

inline float AtmosphericMedium::density(float h, float t0) const
{
    return std::exp(-sqr(_effectiveFalloffScale)*(h*h - _radius*_radius + t0*t0));
}

inline float AtmosphericMedium::densityIntegral(float h, float t0, float t1) const
{
    float s = _effectiveFalloffScale;
    if (t1 == Ray::infinity())
        return (SQRT_PI*0.5f/s)*std::exp((-h*h + _radius*_radius)*s*s)*Erf::erfc(s*t0);
    else
        return (SQRT_PI*0.5f/s)*std::exp((-h*h + _radius*_radius)*s*s)*Erf::erfDifference(s*t0, s*t1);
}

inline float AtmosphericMedium::inverseOpticalDepth(double h, double t0, double sigmaT, double xi) const
{
    double s = _effectiveFalloffScale;
    double inner = std::erf(s*t0) - 2.0*double(INV_SQRT_PI)*std::exp(s*s*(h - _radius)*(h + _radius))*s*std::log(xi)/sigmaT;

    if (inner >= 1.0)
        return Ray::infinity();
    else
        return Erf::erfInv(inner)/s;
}

bool AtmosphericMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    if (state.bounce > _maxBounce)
        return false;

    Vec3f p = (ray.pos() - _center);
    float t0 = p.dot(ray.dir());
    float  h = (p - t0*ray.dir()).length();

    float maxT = ray.farT() + t0;
    if (_absorptionOnly) {
        sample.t = ray.farT();
        sample.weight = std::exp(-_sigmaT*densityIntegral(h, t0, maxT));
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float xi = 1.0f - sampler.next1D();

        float t = inverseOpticalDepth(h, t0, sigmaTc, xi);
        sample.t = min(t, maxT);
        sample.weight = std::exp(-_sigmaT*densityIntegral(h, t0, sample.t));
        sample.exited = (t >= maxT);
        if (sample.exited) {
            sample.pdf = sample.weight.avg();
        } else {
            float rho = density(h, sample.t);
            sample.pdf = (rho*_sigmaT*sample.weight).avg();
            sample.weight *= rho*_sigmaS;
        }
        sample.weight /= sample.pdf;
        sample.t -= t0;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}

Vec3f AtmosphericMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray) const
{
    Vec3f p = (ray.pos() - _center);
    float t0 = p.dot(ray.dir());
    float t1 = ray.farT() + t0;
    float  h = (p - t0*ray.dir()).length();

    return std::exp(-_sigmaT*densityIntegral(h, t0, t1));
}

float AtmosphericMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool onSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        Vec3f p = (ray.pos() - _center);
        float t0 = p.dot(ray.dir());
        float t1 = ray.farT() + t0;
        float  h = (p - t0*ray.dir()).length();

        Vec3f transmittance = std::exp(-_sigmaT*densityIntegral(h, t0, t1));
        if (onSurface) {
            return transmittance.avg();
        } else {
            return (density(h, t0)*_sigmaT*transmittance).avg();
        }
    }
}

Vec3f AtmosphericMedium::transmittanceAndPdfs(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    Vec3f p = (ray.pos() - _center);
    float t0 = p.dot(ray.dir());
    float t1 = ray.farT() + t0;
    float  h = (p - t0*ray.dir()).length();

    Vec3f transmittance = std::exp(-_sigmaT*densityIntegral(h, t0, t1));

    if (_absorptionOnly) {
        pdfForward = pdfBackward = 1.0f;
    } else {
        pdfForward  =   endOnSurface ? transmittance.avg() : (density(h, t1)*_sigmaT*transmittance).avg();
        pdfBackward = startOnSurface ? transmittance.avg() : (density(h, t0)*_sigmaT*transmittance).avg();
    }

    return transmittance;
}

}
