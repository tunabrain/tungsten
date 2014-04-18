#include "Material.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

Material::Material(const rapidjson::Value &v, const Scene &scene)
: JsonSerializable(v),
  _bsdf(scene.fetchBsdf(JsonUtils::fetchMandatoryMember(v, "bsdf"))),
  _emission(JsonUtils::fromJson<float, 3>(v["emission"], Vec3f(0.0f)))
{
    const rapidjson::Value::Member *member;
    if ((member = v.FindMember("colorMap")))
        _base = scene.fetchColorMap(JsonUtils::fromJson<std::string>(member->value));
    if ((member = v.FindMember("alphaMap")))
        _alpha = scene.fetchScalarMap(JsonUtils::fromJson<std::string>(member->value));
    if ((member = v.FindMember("bumpMap")))
        _bump = scene.fetchScalarMap(JsonUtils::fromJson<std::string>(member->value));
}

Material::Material(std::shared_ptr<Bsdf> bsdf, const Vec3f &emission, const std::string &name,
        std::shared_ptr<TextureRgba> base,
        std::shared_ptr<TextureA> alpha,
        std::shared_ptr<TextureA> bump)
: JsonSerializable(name), _bsdf(bsdf), _emission(emission),
  _base(base), _alpha(alpha), _bump(bump)
{
}

rapidjson::Value Material::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("type", "constant", allocator);
    v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
    if (_base)
        v.AddMember("colorMap", _base->path().c_str(), allocator);
    if (_alpha)
        v.AddMember("alphaMap", _alpha->path().c_str(), allocator);
    if (_bump)
        v.AddMember("bumpMap", _bump->path().c_str(), allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
}

}
