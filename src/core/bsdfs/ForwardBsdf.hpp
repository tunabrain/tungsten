#ifndef FORWARDBSDF_HPP_
#define FORWARDBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;
class SurfaceScatterEvent;

class ForwardBsdf : public Bsdf
{
public:
    ForwardBsdf();

    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;
};

}

#endif /* FORWARDBSDF_HPP_ */
