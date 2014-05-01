#ifndef CONDUCTORBSDF_HPP_
#define CONDUCTORBSDF_HPP_


#include <rapidjson/document.h>

#include "Bsdf.hpp"
#include "Fresnel.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SurfaceScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class ConductorBsdf : public Bsdf
{
    Vec3f _eta;
    Vec3f _k;

public:
    ConductorBsdf()
    : _eta(0.214000f, 0.950375f, 1.177500f),
      _k(3.670000f, 2.576500f, 2.160063f)
    {
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "eta", _eta);
        JsonUtils::fromJson(v, "k", _k);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "conductor", allocator);
        v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
        v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
            return false;

        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = 1.0f;
        event.throughput = base(event.info)*Fresnel::conductorReflectance(_eta, _k, event.wi.z());
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
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

    const Vec3f& eta() const {
        return _eta;
    }

    const Vec3f& k() const {
        return _k;
    }
};

}




#endif /* CONDUCTORBSDF_HPP_ */
