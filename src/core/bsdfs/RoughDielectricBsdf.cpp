#include "RoughDielectricBsdf.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

void RoughDielectricBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "distribution", _distributionName);
    JsonUtils::fromJson(v, "enable_refraction", _enableT);

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);

    if (_enableT)
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::GlossyTransmissionLobe);
    else
        _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe);

    init();
}

rapidjson::Value RoughDielectricBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_dielectric", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    v.AddMember("enable_refraction", _enableT, allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}


