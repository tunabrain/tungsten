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

    std::shared_ptr<TextureRgb> _albedo;
    std::shared_ptr<TextureA> _alpha;

    Vec3f albedo(const IntersectionInfo *info) const
    {
        return (*_albedo)[info->uv];
    }

public:
    virtual ~Bsdf()
    {
    }

    Bsdf()
    : _albedo(std::make_shared<ConstantTextureRgb>(Vec3f(1.0f))),
      _alpha(std::make_shared<ConstantTextureA>(1.0f))
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual float alpha(const IntersectionInfo *info) const
    {
        return (*_alpha)[info->uv];
    }

    virtual Vec3f transmittance(const IntersectionInfo */*info*/) const
    {
        return Vec3f(1.0f);
    }

    virtual bool sample(SurfaceScatterEvent &event) const = 0;
    virtual Vec3f eval(const SurfaceScatterEvent &event) const = 0;
    virtual float pdf(const SurfaceScatterEvent &event) const = 0;

    const BsdfLobes &flags() const
    {
        return _lobes;
    }

    void setColor(const std::shared_ptr<TextureRgb> &c)
    {
        _albedo = c;
    }

    void setAlpha(const std::shared_ptr<TextureA> &a)
    {
        _alpha = a;
    }

    std::shared_ptr<TextureRgb> &albedo()
    {
        return _albedo;
    }

    std::shared_ptr<TextureA> &alpha()
    {
        return _alpha;
    }

    const std::shared_ptr<TextureRgb> &albedo() const
    {
        return _albedo;
    }

    const std::shared_ptr<TextureA> &alpha() const
    {
        return _alpha;
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
