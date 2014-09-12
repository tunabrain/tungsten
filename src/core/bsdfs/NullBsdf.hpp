#ifndef NULLBSDF_HPP_
#define NULLBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class Scene;
class SurfaceScatterEvent;

class NullBsdf : public Bsdf
{
public:
    NullBsdf() = default;

    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;
};

}

#endif /* NULLBSDF_HPP_ */
