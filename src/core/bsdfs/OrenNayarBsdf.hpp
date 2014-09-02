#ifndef ORENNAYARBSDF_HPP_
#define ORENNAYARBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class OrenNayarBsdf : public Bsdf
{
    std::shared_ptr<Texture> _roughness;

public:
    OrenNayarBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene);
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    const std::shared_ptr<Texture> &roughness() const
    {
        return _roughness;
    }
};

}

#endif /* ORENNAYARBSDF_HPP_ */
