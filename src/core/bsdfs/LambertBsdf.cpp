#include "LambertBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

LambertBsdf::LambertBsdf()
{
    _lobes = BsdfLobes(BsdfLobes::DiffuseReflectionLobe);
}

rapidjson::Value LambertBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "lambert"
    };
}

bool LambertBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f)
        return false;
    event.wo  = SampleWarp::cosineHemisphere(event.sampler->next2D());
    event.pdf = SampleWarp::cosineHemispherePdf(event.wo);
    event.weight = albedo(event.info);
    event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
    return true;
}

Vec3f LambertBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);
    return albedo(event.info)*INV_PI*event.wo.z();
}

bool LambertBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return false;

    sampler.put2D(SampleWarp::invertCosineHemisphere(event.wo, sampler.untracked1D()));

    return true;
}

float LambertBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;
    return SampleWarp::cosineHemispherePdf(event.wo);
}

}
