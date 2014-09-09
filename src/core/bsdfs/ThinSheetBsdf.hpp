#ifndef THINSHEETBSDF_HPP_
#define THINSHEETBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class ThinSheetBsdf : public Bsdf
{
    float _ior;
    bool _enableInterference;
    std::shared_ptr<Texture> _thickness;
    Vec3f _sigmaA;

public:
    ThinSheetBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    float ior() const
    {
        return _ior;
    }
};

}

#endif /* THINSHEETBSDF_HPP_ */
