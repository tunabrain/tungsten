#include "NullBsdf.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

NullBsdf::NullBsdf()
{
    _lobes = BsdfLobes::NullLobe;
}

rapidjson::Value NullBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "null", allocator);
    return std::move(v);
}

bool NullBsdf::sample(SurfaceScatterEvent &/*event*/) const
{
    return false;
}

Vec3f NullBsdf::eval(const SurfaceScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

float NullBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}
