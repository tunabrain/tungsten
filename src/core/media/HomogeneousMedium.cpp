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

void HomogeneousMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "sigma_a", _materialSigmaA);
    JsonUtils::fromJson(v, "sigma_s", _materialSigmaS);
    JsonUtils::fromJson(v, "density", _density);
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

bool HomogeneousMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    if (state.bounce > _maxBounce)
        return false;

    float maxT = ray.farT();
    if (_absorptionOnly) {
        if (maxT == Ray::infinity())
            return false;
        sample.t = maxT;
        sample.weight = std::exp(-_sigmaT*maxT);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];

        float t = -std::log(1.0f - sampler.next1D())/sigmaTc;
        sample.t = min(t, maxT);
        sample.weight = std::exp(-sample.t*_sigmaT);
        sample.exited = (t >= maxT);
        if (sample.exited) {
            sample.pdf = sample.weight.avg();
        } else {
            sample.pdf = (_sigmaT*sample.weight).avg();
            sample.weight *= _sigmaS;
        }
        sample.weight /= sample.pdf;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}
Vec3f HomogeneousMedium::transmittance(PathSampleGenerator &/*sampler*/, const Ray &ray) const
{
    if (ray.farT() == Ray::infinity())
        return Vec3f(0.0f);
    else {
        return std::exp(-_sigmaT*ray.farT());
    }
}

float HomogeneousMedium::pdf(PathSampleGenerator &/*sampler*/, const Ray &ray, bool onSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        if (onSurface)
            return std::exp(-ray.farT()*_sigmaT).avg();
        else
            return (_sigmaT*std::exp(-ray.farT()*_sigmaT)).avg();
    }
}

Vec3f HomogeneousMedium::transmittanceAndPdfs(PathSampleGenerator &/*sampler*/, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    if (ray.farT() == Ray::infinity()) {
        pdfForward = pdfBackward = 0.0f;
        return Vec3f(0.0f);
    } else if (_absorptionOnly) {
        pdfForward = pdfBackward = 1.0f;
        return std::exp(-_sigmaT*ray.farT());
    } else {
        Vec3f weight = std::exp(-_sigmaT*ray.farT());
        pdfForward  =   endOnSurface ? weight.avg() : (_sigmaT*weight).avg();
        pdfBackward = startOnSurface ? weight.avg() : (_sigmaT*weight).avg();
        return weight;
    }
}

}
