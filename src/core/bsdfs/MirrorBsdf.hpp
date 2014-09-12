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

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;
};

}


#endif /* MIRRORBSDF_HPP_ */
