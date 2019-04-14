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

void AtmosphericMedium::fromJson(JsonPtr value, const Scene &scene)
{
    _scene = &scene;

    Medium::fromJson(value, scene);
    value.getField("pivot", _primName);
    value.getField("sigma_a", _materialSigmaA);
    value.getField("sigma_s", _materialSigmaS);
    value.getField("density", _density);
    value.getField("falloff_scale", _falloffScale);
    value.getField("radius", _radius);
    value.getField("center", _center);
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

Vec3f AtmosphericMedium::sigmaA(Vec3f p) const
{
    return density(p)*_sigmaA;
}

Vec3f AtmosphericMedium::sigmaS(Vec3f p) const
{
    return density(p)*_sigmaS;
}

Vec3f AtmosphericMedium::sigmaT(Vec3f p) const
{
    return density(p)*_sigmaT;
}

inline float AtmosphericMedium::density(Vec3f p) const
{
    return std::exp(-sqr(_effectiveFalloffScale)*((p - _center).lengthSq() - _radius*_radius));
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

inline float AtmosphericMedium::inverseOpticalDepth(double h, double t0, double tau) const
{
    double s = _effectiveFalloffScale;
    double inner = std::erf(s*t0) + 2.0*double(INV_SQRT_PI)*std::exp(s*s*(h - _radius)*(h + _radius))*s*tau;

    if (inner >= 1.0)
        return Ray::infinity();
    else
        return Erf::erfInv(inner)/s;
}

bool AtmosphericMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    sample.emission = Vec3f(0.0f);

    if (state.bounce > _maxBounce)
        return false;

    Vec3f p = (ray.pos() - _center);
    float t0 = p.dot(ray.dir());
    float  h = (p - t0*ray.dir()).length();

    float maxT = ray.farT() + t0;
    if (_absorptionOnly) {
        sample.t = ray.farT();
        Vec3f tau = densityIntegral(h, t0, maxT)*_sigmaT;
        sample.weight = _transmittance->eval(tau, state.firstScatter, true);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float tauC = _transmittance->sample(sampler, state.firstScatter)/sigmaTc;

        float t = inverseOpticalDepth(h, t0, tauC);
        sample.t = min(t, maxT);
        Vec3f tau = densityIntegral(h, t0, sample.t)*_sigmaT;
        sample.weight = _transmittance->eval(tau, state.firstScatter, sample.exited);
        sample.exited = (t >= maxT);
        if (sample.exited) {
            sample.pdf = _transmittance->surfaceProbability(tau, state.firstScatter).avg();
        } else {
            float rho = density(h, sample.t);
            sample.pdf = (rho*_sigmaT*_transmittance->mediumPdf(tau, state.firstScatter)).avg();
            sample.weight *= rho*_sigmaS*_transmittance->sigmaBar();
        }
        sample.weight /= sample.pdf;
        sample.t -= t0;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}

Vec3f AtmosphericMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface) const
{
    Vec3f p = (ray.pos() - _center);
    float t0 = p.dot(ray.dir());
    float t1 = ray.farT() + t0;
    float  h = (p - t0*ray.dir()).length();

    Vec3f tau = _sigmaT*densityIntegral(h, t0, t1);
    return _transmittance->eval(tau, startOnSurface, endOnSurface);
}

float AtmosphericMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface, bool endOnSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        Vec3f p = (ray.pos() - _center);
        float t0 = p.dot(ray.dir());
        float t1 = ray.farT() + t0;
        float  h = (p - t0*ray.dir()).length();


        Vec3f tau = _sigmaT*densityIntegral(h, t0, t1);
        if (endOnSurface)
            return _transmittance->surfaceProbability(tau, startOnSurface).avg();
        else
            return (density(h, t0)*_sigmaT*_transmittance->mediumPdf(tau, startOnSurface)).avg();
    }
}

}
