#ifndef MIRRORBSDF_HPP_
#define MIRRORBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/ScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class MirrorBsdf : public Bsdf
{
public:
    MirrorBsdf()
    {
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "mirror", allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
            return false;
        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = 1.0f;
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
        event.throughput = base(event.info);
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &/*event*/) const override final
    {
        return Vec3f(0.0f);
    }

    float pdf(const SurfaceScatterEvent &/*event*/) const override final
    {
        return 0.0f;
    }
};

}


#endif /* MIRRORBSDF_HPP_ */
