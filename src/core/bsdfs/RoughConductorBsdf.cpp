#include "RoughConductorBsdf.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

void RoughConductorBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "eta", _eta);
    JsonUtils::fromJson(v, "k", _k);
    JsonUtils::fromJson(v, "distribution", _distributionName);

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);

    init();
}

rapidjson::Value RoughConductorBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_conductor", allocator);
    v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
    v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}


