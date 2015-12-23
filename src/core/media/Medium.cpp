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

void Medium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);

    auto phase = v.FindMember("phase_function");
    if (phase != v.MemberEnd())
        _phaseFunction = scene.fetchPhase(phase->value);

    JsonUtils::fromJson(v, "max_bounces", _maxBounce);
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

const PhaseFunction *Medium::phaseFunction(const Vec3f &/*p*/) const
{
    return _phaseFunction.get();
}

}
