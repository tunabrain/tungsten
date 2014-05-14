#include "RoughConductorBsdf.hpp"
#include "ComplexIor.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

void RoughConductorBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    if (JsonUtils::fromJson(v, "eta", _eta) && JsonUtils::fromJson(v, "k", _k))
        _materialName.clear();
    JsonUtils::fromJson(v, "distribution", _distributionName);
    if (JsonUtils::fromJson(v, "material", _materialName)) {
        if (!ComplexIorList::lookup(_materialName, _eta, _k)) {
            DBG("Warning: Unable to find material with name '%s'. Using default", _materialName.c_str());
            ComplexIorList::lookup("Cu", _eta, _k);
        }
    }

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);

    init();
}

rapidjson::Value RoughConductorBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "rough_conductor", allocator);
    if (_materialName.empty()) {
        v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
        v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
    } else {
        v.AddMember("material", _materialName.c_str(), allocator);
    }
    v.AddMember("distribution", _distributionName.c_str(), allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}


