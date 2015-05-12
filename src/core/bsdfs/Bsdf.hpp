#ifndef BSDF_HPP_
#define BSDF_HPP_

#include "BsdfLobes.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "materials/Texture.hpp"

#include "math/TangentFrame.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>
#include <memory>

namespace Tungsten {

class Medium;

class Bsdf : public JsonSerializable
{
protected:
    static CONSTEXPR float DiracAcceptanceThreshold = 1e-3f;

    BsdfLobes _lobes;

    std::shared_ptr<Medium> _intMedium;
    std::shared_ptr<Medium> _extMedium;

    std::shared_ptr<Texture> _albedo;

    Vec3f albedo(const IntersectionInfo *info) const
    {
        return (*_albedo)[*info];
    }

    static inline bool checkReflectionConstraint(const Vec3f &wi, const Vec3f &wo)
    {
        return std::abs(wi.z()*wo.z() - wi.x()*wo.x() - wi.y()*wo.y() - 1.0f) < DiracAcceptanceThreshold;
    }

    static inline bool checkRefractionConstraint(const Vec3f &wi, const Vec3f &wo, float eta, float cosThetaT)
    {
        float dotP = -wi.x()*wo.x()*eta - wi.y()*wo.y()*eta - std::copysign(cosThetaT, wi.z())*wo.z();
        return std::abs(dotP - 1.0f) < DiracAcceptanceThreshold;
    }

public:
    virtual ~Bsdf()
    {
    }

    Bsdf();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool sample(SurfaceScatterEvent &event) const = 0;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const = 0;
    virtual float pdf(const SurfaceScatterEvent &event) const = 0;

    virtual void prepareForRender() {}
    virtual void teardownAfterRender() {}

    const BsdfLobes &lobes() const
    {
        return _lobes;
    }

    void setAlbedo(const std::shared_ptr<Texture> &c)
    {
        _albedo = c;
    }

    std::shared_ptr<Texture> &albedo()
    {
        return _albedo;
    }

    const std::shared_ptr<Texture> &albedo() const
    {
        return _albedo;
    }

    const std::shared_ptr<Medium> &extMedium() const
    {
        return _extMedium;
    }

    const std::shared_ptr<Medium> &intMedium() const
    {
        return _intMedium;
    }

    std::shared_ptr<Medium> &extMedium()
    {
        return _extMedium;
    }

    std::shared_ptr<Medium> &intMedium()
    {
        return _intMedium;
    }

    void setIntMedium(std::shared_ptr<Medium> &intMedium)
    {
        _intMedium = intMedium;
    }

    void setExtMedium(std::shared_ptr<Medium> &extMedium)
    {
        _extMedium = extMedium;
    }

    bool overridesMedia() const
    {
        return _extMedium || _intMedium;
    }

    const Medium *selectMedium(const Medium *currentMedium, bool geometricBackside) const
    {
        if (overridesMedia())
            return geometricBackside ? _intMedium.get() : _extMedium.get();
        else
            return currentMedium;
    }
};

}

#endif /* BSDF_HPP_ */
