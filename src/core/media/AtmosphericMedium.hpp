#ifndef ATMOSPHERICMEDIUM_HPP_
#define ATMOSPHERICMEDIUM_HPP_

#include "Medium.hpp"

#include "materials/Texture.hpp"

#include <memory>

namespace Tungsten {

class AtmosphericMedium : public Medium
{
    const Scene *_scene;
    std::string _primName;

    Vec3f _materialSigmaA, _materialSigmaS;
    float _density;
    float _falloffScale;
    float _radius;
    Vec3f _center;

    float _effectiveFalloffScale;
    Vec3f _sigmaA, _sigmaS;
    Vec3f _sigmaT;
    bool _absorptionOnly;

    inline float density(float h, float t0) const;
    inline float densityIntegral(float h, float t0, float t1) const;
    inline float inverseOpticalDepth(double h, double t0, double sigmaT, double xi) const;

public:
    AtmosphericMedium();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isHomogeneous() const override;

    virtual void prepareForRender() override;

    virtual bool sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
            MediumState &state, MediumSample &sample) const override;
    virtual Vec3f transmittance(PathSampleGenerator &sampler, const Ray &ray) const override;
    virtual float pdf(PathSampleGenerator &sampler, const Ray &ray, bool onSurface) const override;
    virtual Vec3f transmittanceAndPdfs(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
            bool endOnSurface, float &pdfForward, float &pdfBackward) const override;
};

}

#endif /* ATMOSPHERICMEDIUM_HPP_ */
