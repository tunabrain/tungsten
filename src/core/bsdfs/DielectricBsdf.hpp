#ifndef DIELECTRICBSDF_HPP_
#define DIELECTRICBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class Scene;

class DielectricBsdf : public Bsdf
{
    float _ior;

public:
    DielectricBsdf();
    DielectricBsdf(float ior);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    float ior() const
    {
        return _ior;
    }
};

}


#endif /* DIELECTRICBSDF_HPP_ */
