#include "Primitive.hpp"

#include "bsdfs/Bsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void Primitive::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);
    JsonUtils::fromJson(v, "transform", _transform);
    JsonUtils::fromJson(v, "bump_strength", _bumpStrength);
    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));

    const rapidjson::Value::Member *emission = v.FindMember("emission");
    const rapidjson::Value::Member *bump     = v.FindMember("bump");

    if (emission)
        _emission = scene.fetchColorTexture<2>(emission->value);
    if (bump)
        _bump = scene.fetchScalarTexture<2>(bump->value);
}

rapidjson::Value Primitive::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("transform", JsonUtils::toJsonValue(_transform, allocator), allocator);
    v.AddMember("bump_strength", JsonUtils::toJsonValue(_bumpStrength, allocator), allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    if (_emission)
        JsonUtils::addObjectMember(v, "emission", *_emission, allocator);
    if (_bump)
        JsonUtils::addObjectMember(v, "bump", *_bump,  allocator);
    return std::move(v);
}

}
