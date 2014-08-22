#include "ForwardBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

ForwardBsdf::ForwardBsdf()
{
    _lobes = BsdfLobes::ForwardLobe;
}

rapidjson::Value ForwardBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "forward", allocator);
    return std::move(v);
}

bool ForwardBsdf::sample(SurfaceScatterEvent &/*event*/) const
{
    return false;
}

Vec3f ForwardBsdf::eval(const SurfaceScatterEvent &event) const
{
    return (event.requestedLobe.isForward() && -event.wi == event.wo) ? Vec3f(1.0f) : Vec3f(0.0f);
}

float ForwardBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}
