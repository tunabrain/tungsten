#ifndef CHECKERTEXTURE_HPP_
#define CHECKERTEXTURE_HPP_

#include "Texture.hpp"

namespace Tungsten {

class CheckerTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    Vec3f _onColor, _offColor;
    int _resU, _resV;

public:
    CheckerTexture();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec2f &uv) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(const Vec2f &uv) const override;
    virtual float pdf(const Vec2f &uv) const override;

    Vec3f onColor() const
    {
        return _onColor;
    }

    Vec3f offColor() const
    {
        return _offColor;
    }

    int resU() const
    {
        return _resU;
    }

    int resV() const
    {
        return _resV;
    }
};

}

#endif /* CHECKERTEXTURE_HPP_ */
