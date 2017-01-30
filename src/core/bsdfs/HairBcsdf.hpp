#ifndef DEONHAIR_HPP_
#define DEONHAIR_HPP_

#include "PrecomputedAzimuthalLobe.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/Angle.hpp"

#include <memory>

namespace Tungsten {

// An implementation of the papers "An Energy-Conserving Hair Reflectance Model"
// and "Importance Sampling for Physically-Based Hair Fiber Models"
// using precomputed azimuthal scattering functions
class HairBcsdf : public Bsdf
{
    const float Eta = 1.55f;

    float _scaleAngleDeg;
    float _melaninRatio;
    float _melaninConcentration;
    bool _overridesSigmaA;
    Vec3f _sigmaA;
    float _roughness;

    float _scaleAngleRad;
    std::unique_ptr<PrecomputedAzimuthalLobe> _nR, _nTT, _nTRT;
    float _betaR, _betaTT, _betaTRT;
    float _vR, _vTT, _vTRT;

    static float I0(float x);
    static float logI0(float x);

    static float g(float beta, float theta);
    static float D(float beta, float phi);

    static float Phi(float gammaI, float gammaT, int p);

    float M(float v, float sinThetaI, float sinThetaO, float cosThetaI, float cosThetaO) const;

    float NrIntegrand(float beta, float wiDotWo, float phi, float h) const;
    Vec3f NpIntegrand(float beta, float cosThetaD, float phi, int p, float h) const;

    float sampleM(float v, float sinThetaI, float cosThetaI, float xi1, float xi2) const;

    void precomputeAzimuthalDistributions();

public:
    HairBcsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    virtual void prepareForRender() override;
};

}

#endif /* DEONHAIR_HPP_ */
