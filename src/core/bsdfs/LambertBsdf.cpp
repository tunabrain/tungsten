#include "LambertBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

LambertBsdf::LambertBsdf()
{
    _lobes = BsdfLobes(BsdfLobes::DiffuseReflectionLobe);
}

rapidjson::Value LambertBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "lambert", allocator);
    return std::move(v);
}

bool LambertBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f)
        return false;
    event.wo  = SampleWarp::cosineHemisphere(event.sampler->next2D(BsdfSample));
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

float LambertBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;
    return SampleWarp::cosineHemispherePdf(event.wo);
}

}
