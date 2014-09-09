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

public:
    ConductorBsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;

    const Vec3f& eta() const
    {
        return _eta;
    }

    const Vec3f& k() const
    {
        return _k;
    }
};

}




#endif /* CONDUCTORBSDF_HPP_ */
