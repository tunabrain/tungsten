#ifndef MIRRORBSDF_HPP_
#define MIRRORBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class MirrorBsdf : public Bsdf
{
public:
    MirrorBsdf();

    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;
};

}


#endif /* MIRRORBSDF_HPP_ */
