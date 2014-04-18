#include "MixedBsdf.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

MixedBsdf::MixedBsdf(const rapidjson::Value &v, const Scene &scene)
: Bsdf(v),
  _bsdf0(scene.fetchBsdf(JsonUtils::fetchMandatoryMember(v, "bsdf0"))),
  _bsdf1(scene.fetchBsdf(JsonUtils::fetchMandatoryMember(v, "bsdf1"))),
  _ratio(JsonUtils::fromJsonMember<float>(v, "ratio"))
{
    _flags = BsdfFlags(_bsdf0->flags(), _bsdf1->flags());
}

rapidjson::Value MixedBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "mixed", allocator);
    v.AddMember("ratio", _ratio, allocator);
    JsonUtils::addObjectMember(v, "bsdf0", *_bsdf0, allocator);
    JsonUtils::addObjectMember(v, "bsdf1", *_bsdf1, allocator);
    return std::move(v);
}

}


