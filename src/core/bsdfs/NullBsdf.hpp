#ifndef NULLBSDF_HPP_
#define NULLBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class NullBsdf : public Bsdf
{
public:
    NullBsdf()
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "null", allocator);
        return std::move(v);
    }

    bool sample(SurfaceScatterEvent &/*event*/) const override final
    {
        return false;
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

#endif /* NULLBSDF_HPP_ */
