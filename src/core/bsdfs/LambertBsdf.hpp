#ifndef LAMBERTBSDF_HPP_
#define LAMBERTBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;
struct SurfaceScatterEvent;

class LambertBsdf : public Bsdf
{
public:
    LambertBsdf();

    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;
};

}

#endif /* LAMBERTBSDF_HPP_ */
