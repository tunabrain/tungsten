#include "OrenNayarBsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void OrenNayarBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);

    const rapidjson::Value::Member *roughness  = v.FindMember("roughness");
    if (roughness)
        _roughness = scene.fetchScalarTexture<2>(roughness->value);
}

rapidjson::Value OrenNayarBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "oren_nayar", allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}

}
