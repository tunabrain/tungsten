#include "MirrorBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

MirrorBsdf::MirrorBsdf()
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
}

rapidjson::Value MirrorBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "mirror"
    };
}

bool MirrorBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;
    event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
    event.pdf = 1.0f;
    event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
    event.weight = albedo(event.info);
    return true;
}

Vec3f MirrorBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (evalR && checkReflectionConstraint(event.wi, event.wo))
        return albedo(event.info);
    else
        return Vec3f(0.0f);
}

bool MirrorBsdf::invert(WritablePathSampleGenerator &/*sampler*/, const SurfaceScatterEvent &event) const
{
    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (evalR && checkReflectionConstraint(event.wi, event.wo))
        return true;
    else
        return false;
}

float MirrorBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (sampleR && checkReflectionConstraint(event.wi, event.wo))
        return 1.0f;
    else
        return 0.0f;
}

}
