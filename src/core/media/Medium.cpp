#include "Medium.hpp"

#include "transmittances/ExponentialTransmittance.hpp"

#include "phasefunctions/IsotropicPhaseFunction.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

Medium::Medium()
: _transmittance(std::make_shared<ExponentialTransmittance>()),
  _phaseFunction(std::make_shared<IsotropicPhaseFunction>()),
  _maxBounce(1024)
{
}

void Medium::fromJson(JsonPtr value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);

    if (auto phase = value["phase_function"])
        _phaseFunction = scene.fetchPhase(phase);
    if (auto trans = value["transmittance"])
        _transmittance = scene.fetchTransmittance(trans);

    value.getField("max_bounces", _maxBounce);
}

rapidjson::Value Medium::toJson(Allocator &allocator) const
{
    return JsonObject{JsonSerializable::toJson(allocator), allocator,
        "phase_function", *_phaseFunction,
        "transmittance", *_transmittance,
        "max_bounces", _maxBounce
    };
}

bool Medium::invertDistance(WritablePathSampleGenerator &/*sampler*/, const Ray &/*ray*/, bool /*onSurface*/) const
{
    FAIL("Medium::invert not implemented!");
}

Vec3f Medium::transmittanceAndPdfs(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    pdfForward  = pdf(sampler, ray, startOnSurface, endOnSurface);
    pdfBackward = pdf(sampler, ray.scatter(ray.hitpoint(), -ray.dir(), 0.0f, ray.farT()), endOnSurface, startOnSurface);
    return transmittance(sampler, ray, startOnSurface, endOnSurface);
}

const PhaseFunction *Medium::phaseFunction(const Vec3f &/*p*/) const
{
    return _phaseFunction.get();
}

bool Medium::isDirac() const
{
    return _transmittance->isDirac();
}

}
