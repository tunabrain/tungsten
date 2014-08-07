#ifndef LAMBERTBSDF_HPP_
#define LAMBERTBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class LambertBsdf : public Bsdf
{
public:
    LambertBsdf()
    {
        _lobes = BsdfLobes(BsdfLobes::DiffuseReflectionLobe);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "lambert", allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return false;
        if (event.wi.z() <= 0.0f)
            return false;
        event.wo  = Sample::cosineHemisphere(event.sampler->next2D());
        event.pdf = Sample::cosineHemispherePdf(event.wo);
        event.throughput = albedo(event.info);
        event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return Vec3f(0.0f);
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return Vec3f(0.0f);
        return albedo(event.info)*INV_PI*event.wo.z();
    }

    float pdf(const SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
            return 0.0f;
        if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
            return 0.0f;
        return Sample::cosineHemispherePdf(event.wo);
    }
};

}

#endif /* LAMBERTBSDF_HPP_ */
