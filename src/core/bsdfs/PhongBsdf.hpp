#ifndef PHONGBSDF_HPP_
#define PHONGBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten
{

class Scene;

class PhongBsdf : public Bsdf
{
    float _exponent;
    float _invExponent;
    float _pdfFactor;
    float _brdfFactor;

    void init();

public:
    PhongBsdf(float exponent = 64.0f);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    float exponent() const
    {
        return _exponent;
    }
};

}


#endif /* PHONGBSDF_HPP_ */
