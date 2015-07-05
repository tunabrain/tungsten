#include "Bsdf.hpp"

#include "materials/ConstantTexture.hpp"

#include "volume/Medium.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

Bsdf::Bsdf()
: _albedo(std::make_shared<ConstantTexture>(1.0f))
{
}

void Bsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);

    scene.textureFromJsonMember(v, "albedo", TexelConversion::REQUEST_RGB, _albedo);
    scene.textureFromJsonMember(v, "bump", TexelConversion::REQUEST_AVERAGE, _bump);
}

rapidjson::Value Bsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));

    JsonUtils::addObjectMember(v, "albedo", *_albedo,  allocator);
    if (_bump)
        JsonUtils::addObjectMember(v, "bump", *_bump,  allocator);

    return std::move(v);
}

}
