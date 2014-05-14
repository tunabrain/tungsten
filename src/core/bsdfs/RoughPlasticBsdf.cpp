#include "RoughPlasticBsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void RoughPlasticBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "distribution", _distributionName);
    JsonUtils::fromJson(v, "thickness", _thickness);
    JsonUtils::fromJson(v, "sigmaA", _sigmaA);

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);

    init();
}

rapidjson::Value RoughPlasticBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_plastic", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("thickness", _thickness, allocator);
    v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}
