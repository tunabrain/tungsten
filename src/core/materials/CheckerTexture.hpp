#ifndef CHECKERTEXTURE_HPP_
#define CHECKERTEXTURE_HPP_

#include "Texture.hpp"

namespace Tungsten {

class CheckerTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    Vec3f _onColor, _offColor;
    int _resU, _resV;
    float _offsetU, _offsetV;

public:
    CheckerTexture();
    CheckerTexture(Vec3f onColor, Vec3f offColor, int resU, int resV, float offsetU, float offsetV);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec2f &uv) const override final;
    virtual Vec3f operator[](const IntersectionInfo &info) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const override;

    virtual void scaleValues(float factor) override;

    virtual Texture *clone() const override;

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

    float offsetU() const
    {
        return _offsetU;
    }

    float offsetV() const
    {
        return _offsetV;
    }

    void setOffColor(Vec3f offColor)
    {
        _offColor = offColor;
    }

    void setOnColor(Vec3f onColor)
    {
        _onColor = onColor;
    }

    void setResU(int resU)
    {
        _resU = resU;
    }

    void setResV(int resV)
    {
        _resV = resV;
    }

    void setOffsetU(float offsetU)
    {
        _offsetU = offsetU;
    }

    void setOffsetV(float offsetV)
    {
        _offsetV = offsetV;
    }

};

}

#endif /* CHECKERTEXTURE_HPP_ */
