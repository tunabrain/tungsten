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
    BsdfLobes _lobes;

    std::shared_ptr<Medium> _intMedium;
    std::shared_ptr<Medium> _extMedium;

    std::shared_ptr<Texture> _albedo;

    Vec3f albedo(const IntersectionInfo *info) const
    {
        return (*_albedo)[*info];
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

    bool overridesMedia() const
    {
        return _extMedium || _intMedium;
    }
};

}

#endif /* BSDF_HPP_ */
