#ifndef LAMBERTBSDF_HPP_
#define LAMBERTBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class Scene;
class SurfaceScatterEvent;

class LambertBsdf : public Bsdf
{
public:
    LambertBsdf();

    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;
};

}

#endif /* LAMBERTBSDF_HPP_ */
