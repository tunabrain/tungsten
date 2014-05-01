#include "Bsdf.hpp"

#include "volume/Medium.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void Bsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);

    JsonUtils::fromJson(v, "bumpStrength", _bumpStrength);

    const rapidjson::Value::Member *intMedium = v.FindMember("intMedium");
    const rapidjson::Value::Member *extMedium = v.FindMember("extMedium");

    const rapidjson::Value::Member *base  = v.FindMember("color");
    const rapidjson::Value::Member *alpha = v.FindMember("alpha");
    const rapidjson::Value::Member *bump  = v.FindMember("bump");

    if (intMedium)
        _intMedium = scene.fetchMedium(intMedium->value);
    if (extMedium)
        _extMedium = scene.fetchMedium(extMedium->value);
    if (base)
        _base = scene.fetchColorTexture<2>(base->value);
    if (alpha)
        _alpha = scene.fetchScalarTexture<2>(alpha->value);
    if (bump)
        _bump = scene.fetchScalarTexture<2>(bump->value);
}

rapidjson::Value Bsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));

    v.AddMember("bumpStrength", JsonUtils::toJsonValue(_bumpStrength, allocator), allocator);
    if (_intMedium)
        JsonUtils::addObjectMember(v, "intMedium", *_intMedium,  allocator);
    if (_extMedium)
        JsonUtils::addObjectMember(v, "extMedium", *_extMedium, allocator);
    JsonUtils::addObjectMember(v, "color", *_base,  allocator);
    JsonUtils::addObjectMember(v, "alpha", *_alpha, allocator);
    JsonUtils::addObjectMember(v,  "bump", *_bump,  allocator);

    return std::move(v);
}

}
