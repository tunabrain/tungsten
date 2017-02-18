#include "Bsdf.hpp"

#include "textures/ConstantTexture.hpp"

#include "media/Medium.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

std::shared_ptr<Texture> Bsdf::_defaultAlbedo = std::make_shared<ConstantTexture>(1.0f);

Bsdf::Bsdf()
: _albedo(_defaultAlbedo)
{
}

void Bsdf::fromJson(JsonPtr value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);

    if (auto albedo = value["albedo"]) _albedo = scene.fetchTexture(albedo, TexelConversion::REQUEST_RGB);
    if (auto bump   = value["bump"  ]) _bump   = scene.fetchTexture(bump,   TexelConversion::REQUEST_AVERAGE);
}

rapidjson::Value Bsdf::toJson(Allocator &allocator) const
{
    JsonObject result{JsonSerializable::toJson(allocator), allocator,
        "albedo", *_albedo
    };
    if (_bump)
        result.add("bump", *_bump);

    return result;
}

bool Bsdf::invert(WritablePathSampleGenerator &/*sampler*/, const SurfaceScatterEvent &/*event*/) const
{
    FAIL("Invert not implemented!");
}

}
