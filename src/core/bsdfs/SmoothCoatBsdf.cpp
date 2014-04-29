#include "SmoothCoatBsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten
{

void SmoothCoatBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigmaA", _sigmaA);
    _substrate = scene.fetchBsdf(JsonUtils::fetchMember(v, "substrate"));

    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe, _substrate->flags());
    init();
}

}
