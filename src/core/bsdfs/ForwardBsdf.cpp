#include "ForwardBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten
{

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

bool ForwardBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::ForwardLobe))
        return false;
    event.wo = -event.wi;
    event.throughput = Vec3f(1.0f);
    event.pdf = 1.0f;
    event.sampledLobe = BsdfLobes::ForwardLobe;
    return true;
}

Vec3f ForwardBsdf::eval(const SurfaceScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

float ForwardBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}
