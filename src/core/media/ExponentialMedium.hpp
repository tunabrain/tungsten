#ifndef EXPONENTIALMEDIUM_HPP_
#define EXPONENTIALMEDIUM_HPP_

#include "Medium.hpp"

namespace Tungsten {

class ExponentialMedium : public Medium
{
    Vec3f _materialSigmaA, _materialSigmaS;
    float _density;
    float _falloffScale;
    Vec3f _unitPoint;
    Vec3f _falloffDirection;

    Vec3f _unitFalloffDirection;
    Vec3f _sigmaA, _sigmaS;
    Vec3f _sigmaT;
    bool _absorptionOnly;

    inline float density(Vec3f p) const;
    inline float density(float x, float dx, float t) const;
    inline float densityIntegral(float x, float dx, float tMax) const;
    inline float inverseOpticalDepth(float x, float dx, float tau) const;

public:
    ExponentialMedium();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isHomogeneous() const override;

    virtual void prepareForRender() override;

    virtual Vec3f sigmaA(Vec3f p) const override;
    virtual Vec3f sigmaS(Vec3f p) const override;
    virtual Vec3f sigmaT(Vec3f p) const override;

    virtual bool sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
            MediumState &state, MediumSample &sample) const override;
    virtual Vec3f transmittance(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
            bool endOnSurface) const override;
    virtual float pdf(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface, bool endOnSurface) const override;
};

}



#endif /* EXPONENTIALMEDIUM_HPP_ */
