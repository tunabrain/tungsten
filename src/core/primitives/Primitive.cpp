#include "Primitive.hpp"

#include "bsdfs/Bsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void Primitive::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);
    JsonUtils::fromJson(v, "transform", _transform);
    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));

    const rapidjson::Value::Member *emission  = v.FindMember("emission");
    if (emission)
        _emission = scene.fetchColorTexture<2>(emission->value);
}

rapidjson::Value Primitive::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("transform", JsonUtils::toJsonValue(_transform, allocator), allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    if (_emission)
        JsonUtils::addObjectMember(v, "emission", *_emission, allocator);
    return std::move(v);
}

}
