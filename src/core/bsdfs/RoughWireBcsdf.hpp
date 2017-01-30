#ifndef ROUGHWIREBCSDF_HPP_
#define ROUGHWIREBCSDF_HPP_

#include "bsdfs/Bsdf.hpp"

namespace Tungsten {

class RoughWireBcsdf : public Bsdf
{
    std::string _materialName;
    Vec3f _eta;
    Vec3f _k;
    float _roughness;

    float _v;

    bool lookupMaterial();

    static float I0(float x);
    static float logI0(float x);

    float N(float cosPhi) const;
    float M(float v, float sinThetaI, float sinThetaO, float cosThetaI, float cosThetaO) const;

    float sampleN(float xi) const;
    float sampleM(float v, float sinThetaI, float cosThetaI, float xi1, float xi2) const;

public:
    RoughWireBcsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;
};

}

#endif /* ROUGHWIREBCSDF_HPP_ */
