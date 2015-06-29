#include "HomogeneousMedium.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/TangentFrame.hpp"
#include "math/Ray.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

HomogeneousMedium::HomogeneousMedium()
: _sigmaA(0.0f),
  _sigmaS(0.0f)
{
    init();
}

void HomogeneousMedium::init()
{
    _sigmaT = _sigmaA + _sigmaS;
    _albedo = _sigmaS/_sigmaT;
    _maxAlbedo = _albedo.max();
    _absorptionWeight = 1.0f/_maxAlbedo;
    _absorptionOnly = _maxAlbedo == 0.0f;
}

void HomogeneousMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "sigma_a", _sigmaA);
    JsonUtils::fromJson(v, "sigma_s", _sigmaS);

    init();
}

rapidjson::Value HomogeneousMedium::toJson(Allocator &allocator) const
{
    rapidjson::Value v(Medium::toJson(allocator));
    v.AddMember("type", "homogeneous", allocator);
    v.AddMember("sigma_a", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    v.AddMember("sigma_s", JsonUtils::toJsonValue(_sigmaS, allocator), allocator);

    return std::move(v);
}

bool HomogeneousMedium::isHomogeneous() const
{
    return true;
}

void HomogeneousMedium::prepareForRender()
{
}

void HomogeneousMedium::teardownAfterRender()
{
}

bool HomogeneousMedium::sampleDistance(VolumeScatterEvent &event, MediumState &state) const
{
    if (state.bounce > _maxBounce)
        return false;

    if (_absorptionOnly) {
        if (event.maxT == Ray::infinity())
            return false;
        event.t = event.maxT;
        event.weight = std::exp(-_sigmaT*event.t);
    } else {
        int component = event.sampler->nextDiscrete(DiscreteTransmittanceSample, 3);
        float sigmaTc = _sigmaT[component];

        float t = -std::log(1.0f - event.sampler->next1D(TransmittanceSample))/sigmaTc;
        event.t = min(t, event.maxT);
        event.weight = std::exp(-event.t*_sigmaT);
        if (t < event.maxT)
            event.weight /= (_sigmaT*event.weight).avg();
        else
            event.weight /= event.weight.avg();

        state.advance();
    }

    return true;
}

bool HomogeneousMedium::absorb(VolumeScatterEvent &event, MediumState &/*state*/) const
{
    if (event.sampler->nextBoolean(DiscreteMediumSample, 1.0f - _maxAlbedo))
        return true;
    event.weight = Vec3f(_absorptionWeight);
    return false;
}

bool HomogeneousMedium::scatter(VolumeScatterEvent &event) const
{
    event.wo = PhaseFunction::sample(_phaseFunction, _phaseG, event.sampler->next2D(MediumSample));
    event.pdf = PhaseFunction::eval(_phaseFunction, event.wo.z(), _phaseG);
    event.weight *= _sigmaS;
    TangentFrame frame(event.wi);
    event.wo = frame.toGlobal(event.wo);
    return true;
}

Vec3f HomogeneousMedium::transmittance(const VolumeScatterEvent &event) const
{
    if (event.t == Ray::infinity())
        return Vec3f(0.0f);
    else
        return std::exp(-_sigmaT*event.t);
}

Vec3f HomogeneousMedium::emission(const VolumeScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

Vec3f HomogeneousMedium::phaseEval(const VolumeScatterEvent &event) const
{
    return _sigmaS*PhaseFunction::eval(_phaseFunction, event.wi.dot(event.wo), _phaseG);
}

}
