#include "RoughCoatBsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten
{

void RoughCoatBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigmaA", _sigmaA);
    JsonUtils::fromJson(v, "distribution", _distributionName);
    _substrate = scene.fetchBsdf(JsonUtils::fetchMember(v, "substrate"));

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);

    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe, _substrate->flags());
    init();
}

rapidjson::Value RoughCoatBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_coat", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("thickness", _thickness, allocator);
    v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    JsonUtils::addObjectMember(v, "substrate", *_substrate, allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}
