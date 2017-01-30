#ifndef DISKTEXTURE_HPP_
#define DISKTEXTURE_HPP_

#include "Texture.hpp"

namespace Tungsten {

class DiskTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    Vec3f _value;

public:
    DiskTexture();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec<float, 2> &uv) const override final;
    virtual Vec3f operator[](const IntersectionInfo &info) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const override;

    virtual void scaleValues(float factor) override;

    virtual Texture *clone() const override;
};

}

#endif /* DISKTEXTURE_HPP_ */
