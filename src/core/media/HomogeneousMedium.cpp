#include "HomogeneousMedium.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/TangentFrame.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

HomogeneousMedium::HomogeneousMedium()
: _materialSigmaA(0.0f),
  _materialSigmaS(0.0f),
  _density(1.0f)
{
}

void HomogeneousMedium::fromJson(JsonPtr value, const Scene &scene)
{
    Medium::fromJson(value, scene);
    value.getField("sigma_a", _materialSigmaA);
    value.getField("sigma_s", _materialSigmaS);
    value.getField("density", _density);
}

rapidjson::Value HomogeneousMedium::toJson(Allocator &allocator) const
{
    return JsonObject{Medium::toJson(allocator), allocator,
        "type", "homogeneous",
        "sigma_a", _materialSigmaA,
        "sigma_s", _materialSigmaS,
        "density", _density
    };
}

bool HomogeneousMedium::isHomogeneous() const
{
    return true;
}

void HomogeneousMedium::prepareForRender()
{
    _sigmaA = _materialSigmaA*_density;
    _sigmaS = _materialSigmaS*_density;
    _sigmaT = _sigmaA + _sigmaS;
    _absorptionOnly = _sigmaS == 0.0f;
}

Vec3f HomogeneousMedium::sigmaA(Vec3f /*p*/) const
{
    return _sigmaA;
}

Vec3f HomogeneousMedium::sigmaS(Vec3f /*p*/) const
{
    return _sigmaS;
}

Vec3f HomogeneousMedium::sigmaT(Vec3f /*p*/) const
{
    return _sigmaT;
}

bool HomogeneousMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    sample.emission = Vec3f(0.0f);

    if (state.bounce > _maxBounce)
        return false;

    float maxT = ray.farT();
    if (_absorptionOnly) {
        if (maxT == Ray::infinity())
            return false;
        sample.t = maxT;
        sample.weight = _transmittance->eval(sample.t*_sigmaT, state.firstScatter, true);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];

        float t = _transmittance->sample(sampler, state.firstScatter)/sigmaTc;
        sample.t = min(t, maxT);
        sample.continuedT = t;
        sample.exited = (t >= maxT);
        Vec3f tau = sample.t*_sigmaT;
        Vec3f continuedTau = sample.continuedT*_sigmaT;
        sample.weight = _transmittance->eval(tau, state.firstScatter, sample.exited);
        sample.continuedWeight = _transmittance->eval(continuedTau, state.firstScatter, sample.exited);
        if (sample.exited) {
            sample.pdf = _transmittance->surfaceProbability(tau, state.firstScatter).avg();
        } else {
            sample.pdf = (_sigmaT*_transmittance->mediumPdf(tau, state.firstScatter)).avg();
            sample.weight *= _sigmaS*_transmittance->sigmaBar();
        }
        sample.weight /= sample.pdf;
        sample.continuedWeight = _sigmaS*_transmittance->sigmaBar()*sample.continuedWeight/(_sigmaT*_transmittance->mediumPdf(continuedTau, state.firstScatter)).avg();

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}

Vec3f HomogeneousMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface) const
{
    if (ray.farT() == Ray::infinity())
        return Vec3f(0.0f);
    else
        return _transmittance->eval(_sigmaT*ray.farT(), startOnSurface, endOnSurface);
}

float HomogeneousMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface, bool endOnSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        Vec3f tau = ray.farT()*_sigmaT;
        if (endOnSurface)
            return _transmittance->surfaceProbability(tau, startOnSurface).avg();
        else
            return (_sigmaT*_transmittance->mediumPdf(tau, startOnSurface)).avg();
    }
}

}
