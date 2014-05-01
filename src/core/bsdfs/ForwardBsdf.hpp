#ifndef FORWARDBSDF_HPP_
#define FORWARDBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/SurfaceScatterEvent.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class ForwardBsdf : public Bsdf
{
public:
    ForwardBsdf()
    {
        _lobes = BsdfLobes::ForwardLobe;
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "forward", allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::SpecularTransmissionLobe))
            return false;
        event.wo = -event.wi;
        event.throughput = Vec3f(1.0f);
        event.pdf = 1.0f;
        event.sampledLobe = BsdfLobes::SpecularTransmissionLobe;
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

#endif /* FORWARDBSDF_HPP_ */
