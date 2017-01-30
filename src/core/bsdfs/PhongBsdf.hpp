#ifndef PHONGBSDF_HPP_
#define PHONGBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class PhongBsdf : public Bsdf
{
    float _exponent;
    float _invExponent;
    float _pdfFactor;
    float _brdfFactor;
    float _diffuseRatio;

public:
    PhongBsdf(float exponent = 64.0f, float diffuseRatio = 0.2f);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;

    float exponent() const
    {
        return _exponent;
    }

    float diffuseRatio() const
    {
        return _diffuseRatio;
    }

    void setDiffuseRatio(float diffuseRatio)
    {
        _diffuseRatio = diffuseRatio;
    }

    void setExponent(float exponent)
    {
        _exponent = exponent;
    }
};

}


#endif /* PHONGBSDF_HPP_ */
