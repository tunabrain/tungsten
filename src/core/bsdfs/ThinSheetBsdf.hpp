#ifndef THINSHEETBSDF_HPP_
#define THINSHEETBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class ThinSheetBsdf : public Bsdf
{
    static constexpr bool UseAlphaTrick = true;

    float _ior;
    std::shared_ptr<TextureA> _thickness;
    Vec3f _sigmaA;

public:
    ThinSheetBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual float alpha(const IntersectionInfo *info) const override final;
    virtual Vec3f transmittance(const IntersectionInfo *info) const override final;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    float ior() const
    {
        return _ior;
    }
};

}

#endif /* THINSHEETBSDF_HPP_ */
