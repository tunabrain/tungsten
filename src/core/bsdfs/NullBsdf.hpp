#ifndef NULLBSDF_HPP_
#define NULLBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;
class SurfaceScatterEvent;

class NullBsdf : public Bsdf
{
public:
    NullBsdf() = default;

    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;
};

}

#endif /* NULLBSDF_HPP_ */
