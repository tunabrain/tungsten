#include "Medium.hpp"

#include "phasefunctions/IsotropicPhaseFunction.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

Medium::Medium()
: _phaseFunction(std::make_shared<IsotropicPhaseFunction>()),
  _maxBounce(1024)
{
}

void Medium::fromJson(JsonPtr value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);

    if (auto phase = value["phase_function"])
        _phaseFunction = scene.fetchPhase(phase);

    value.getField("max_bounces", _maxBounce);
}

rapidjson::Value Medium::toJson(Allocator &allocator) const
{
    return JsonObject{JsonSerializable::toJson(allocator), allocator,
        "phase_function", *_phaseFunction,
        "max_bounces", _maxBounce
    };
}

Vec3f Medium::transmittanceAndPdfs(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    pdfForward = pdf(sampler, ray, endOnSurface);
    pdfBackward = pdf(sampler, ray.scatter(ray.hitpoint(), -ray.dir(), 0.0f, ray.farT()), startOnSurface);
    return transmittance(sampler, ray);
}

bool Medium::invert(WritablePathSampleGenerator &/*sampler*/, const Ray &/*ray*/, bool /*onSurface*/) const
{
    FAIL("Medium::invert not implemented!");
}

const PhaseFunction *Medium::phaseFunction(const Vec3f &/*p*/) const
{
    return _phaseFunction.get();
}

}
