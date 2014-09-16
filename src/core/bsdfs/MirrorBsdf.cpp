#include "MirrorBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

MirrorBsdf::MirrorBsdf()
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
}

rapidjson::Value MirrorBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "mirror", allocator);
    return std::move(v);
}

bool MirrorBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;
    event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
    event.pdf = 0.0f;
    event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
    event.throughput = albedo(event.info);
    return true;
}

Vec3f MirrorBsdf::eval(const SurfaceScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

float MirrorBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}
