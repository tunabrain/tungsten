#ifndef CONDUCTORBSDF_HPP_
#define CONDUCTORBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class Scene;

class ConductorBsdf : public Bsdf
{
    std::string _materialName;
    Vec3f _eta;
    Vec3f _k;

    bool lookupMaterial();

public:
    ConductorBsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    Vec3f eta() const
    {
        return _eta;
    }

    Vec3f k() const
    {
        return _k;
    }

    const std::string &materialName() const
    {
        return _materialName;
    }

    void setEta(Vec3f eta)
    {
        _eta = eta;
    }

    void setK(Vec3f k)
    {
        _k = k;
    }

    void setMaterialName(const std::string &materialName)
    {
        const std::string &oldMaterial = _materialName;
        _materialName = materialName;
        if (!lookupMaterial())
            _materialName = oldMaterial;
    }
};

}




#endif /* CONDUCTORBSDF_HPP_ */
