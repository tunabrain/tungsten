#ifndef HOMOGENEOUSMEDIUM_HPP_
#define HOMOGENEOUSMEDIUM_HPP_

#include "Medium.hpp"

namespace Tungsten {

class HomogeneousMedium : public Medium
{
    Vec3f _sigmaA, _sigmaS;

    Vec3f _sigmaT;
    Vec3f _albedo;
    float _maxAlbedo;
    float _absorptionWeight;
    bool _absorptionOnly;

    void init();

public:
    HomogeneousMedium();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isHomogeneous() const override;

    virtual void prepareForRender() override;
    virtual void cleanupAfterRender() override;

    virtual bool sampleDistance(VolumeScatterEvent &event, MediumState &state) const override;
    virtual bool absorb(VolumeScatterEvent &event, MediumState &state) const override;
    virtual bool scatter(VolumeScatterEvent &event) const override;
    virtual Vec3f transmittance(const VolumeScatterEvent &event) const override;
    virtual Vec3f emission(const VolumeScatterEvent &event) const override;

    virtual Vec3f phaseEval(const VolumeScatterEvent &event) const override;

    Vec3f sigmaA() const { return _sigmaA; }
    Vec3f sigmaS() const { return _sigmaS; }
};

}



#endif /* HOMOGENEOUSMEDIUM_HPP_ */
