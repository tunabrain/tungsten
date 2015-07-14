#include "Medium.hpp"

#include "phasefunctions/IsotropicPhaseFunction.hpp"

#include "io/JsonUtils.hpp"
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

    const rapidjson::Value::Member *phase = v.FindMember("phase_function");
    if (phase)
        _phaseFunction = scene.fetchPhase(phase->value);

    JsonUtils::fromJson(v, "max_bounces", _maxBounce);
}

rapidjson::Value Medium::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));
    JsonUtils::addObjectMember(v, "phase_function", *_phaseFunction, allocator);
    v.AddMember("max_bounces", _maxBounce, allocator);

    return std::move(v);
}

Vec3f Medium::transmittanceAndPdfs(const Ray &ray, bool startOnSurface, bool endOnSurface,
        float &pdfForward, float &pdfBackward) const
{
    pdfForward = pdf(ray, endOnSurface);
    pdfBackward = pdf(ray.scatter(ray.hitpoint(), -ray.dir(), 0.0f, ray.farT()), startOnSurface);
    return transmittance(ray);
}

const PhaseFunction *Medium::phaseFunction(const Vec3f &/*p*/) const
{
    return _phaseFunction.get();
}

}
