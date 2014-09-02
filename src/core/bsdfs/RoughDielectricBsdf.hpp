#ifndef ROUGHDIELECTRICBSDF_HPP_
#define ROUGHDIELECTRICBSDF_HPP_

#include "Microfacet.hpp"
#include "Bsdf.hpp"

namespace Tungsten {

class RoughDielectricBsdf : public Bsdf
{
    std::string _distributionName;

    std::shared_ptr<Texture> _roughness;
    float _ior;
    bool _enableT;

    Microfacet::Distribution _distribution;

    void init();

public:

    RoughDielectricBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    static bool sampleBase(SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    static Vec3f evalBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    static float pdfBase(const SurfaceScatterEvent &event, bool sampleR, bool sampleT,
            float roughness, float ior, Microfacet::Distribution distribution);

    bool sample(SurfaceScatterEvent &event) const override final;
    Vec3f eval(const SurfaceScatterEvent &event) const override final;
    float pdf(const SurfaceScatterEvent &event) const override final;

    float ior() const {
        return _ior;
    }

    const std::shared_ptr<Texture> &roughness() const
    {
        return _roughness;
    }

    const std::string &distributionName() const
    {
        return _distributionName;
    }
};

}


#endif /* ROUGHDIELECTRICBSDF_HPP_ */
