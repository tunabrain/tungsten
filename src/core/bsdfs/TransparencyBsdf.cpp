#include "TransparencyBsdf.hpp"
#include "LambertBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "materials/ConstantTexture.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

TransparencyBsdf::TransparencyBsdf()
: _opacity(std::make_shared<ConstantTexture>(1.0f)),
  _base(std::make_shared<LambertBsdf>())
{
    _lobes = BsdfLobes(BsdfLobes::ForwardLobe, _base->lobes());
}

TransparencyBsdf::TransparencyBsdf(std::shared_ptr<Texture> opacity, std::shared_ptr<Bsdf> base)
: _opacity(opacity),
  _base(base)
{
    _lobes = BsdfLobes(BsdfLobes::ForwardLobe, _base->lobes());
}

void TransparencyBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    _base = scene.fetchBsdf(JsonUtils::fetchMember(v, "base"));

    const rapidjson::Value::Member *opacity = v.FindMember("opacity");
    if (opacity)
        _opacity = scene.fetchTexture(opacity->value, true);

    _lobes = BsdfLobes(BsdfLobes::ForwardLobe, _base->lobes());
}

rapidjson::Value TransparencyBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));

    v.AddMember("type", "transparency", allocator);
    JsonUtils::addObjectMember(v, "base", *_base,  allocator);
    JsonUtils::addObjectMember(v, "opacity", *_opacity,  allocator);

    return std::move(v);
}

bool TransparencyBsdf::sample(SurfaceScatterEvent &event) const
{
    return _base->sample(event);
}

Vec3f TransparencyBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (event.requestedLobe.isForward())
        return (-event.wi == event.wo) ? Vec3f(1.0f - (*_opacity)[event.info->uv].x()) : Vec3f(0.0f);
    else
        return _base->eval(event);
}

float TransparencyBsdf::pdf(const SurfaceScatterEvent &event) const
{
    return _base->pdf(event);
}

}
