#include "Bsdf.hpp"

#include "volume/Medium.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void Bsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);

    const rapidjson::Value::Member *intMedium = v.FindMember("int_medium");
    const rapidjson::Value::Member *extMedium = v.FindMember("ext_medium");

    const rapidjson::Value::Member *base  = v.FindMember("albedo");
    const rapidjson::Value::Member *alpha = v.FindMember("alpha");

    if (intMedium)
        _intMedium = scene.fetchMedium(intMedium->value);
    if (extMedium)
        _extMedium = scene.fetchMedium(extMedium->value);
    if (base)
        _albedo = scene.fetchColorTexture<2>(base->value);
    if (alpha)
        _alpha = scene.fetchScalarTexture<2>(alpha->value);
}

rapidjson::Value Bsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));

    if (_intMedium)
        JsonUtils::addObjectMember(v, "int_medium", *_intMedium,  allocator);
    if (_extMedium)
        JsonUtils::addObjectMember(v, "ext_medium", *_extMedium, allocator);
    JsonUtils::addObjectMember(v, "albedo", *_albedo,  allocator);
    JsonUtils::addObjectMember(v, "alpha", *_alpha, allocator);

    return std::move(v);
}

}
