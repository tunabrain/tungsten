#include "MixedBsdf.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

void MixedBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);

    _bsdf0 = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf0"));
    _bsdf1 = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf1"));
    _lobes = BsdfLobes(_bsdf0->flags(), _bsdf1->flags());

    const rapidjson::Value::Member *ratio  = v.FindMember("ratio");
    if (ratio)
        _ratio = scene.fetchScalarTexture<2>(ratio->value);
}

rapidjson::Value MixedBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "mixed", allocator);
    JsonUtils::addObjectMember(v, "bsdf0", *_bsdf0, allocator);
    JsonUtils::addObjectMember(v, "bsdf1", *_bsdf1, allocator);
    JsonUtils::addObjectMember(v, "ratio", *_ratio, allocator);
    return std::move(v);
}

}


