#ifndef THINSHEETBSDF_HPP_
#define THINSHEETBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

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

class ThinSheetBsdf : public Bsdf
{
    float _ior;

public:
    ThinSheetBsdf()
    : _ior(1.5f)
    {
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "ior", _ior);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "thinsheet", allocator);
        v.AddMember("ior", _ior, allocator);
        return std::move(v);
    }

    virtual float alpha(const IntersectionInfo *info) const override final
    {
        float R = Fresnel::dielectricReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)));
        if (R < 1.0f)
            R += (1.0f - R)*(1.0f - R)*R/(1.0f - R*R);
        return R;
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


#endif /* THINSHEETBSDF_HPP_ */
