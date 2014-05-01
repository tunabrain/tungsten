#ifndef BSDF_HPP_
#define BSDF_HPP_

#include "BsdfLobes.hpp"

#include "primitives/Primitive.hpp"

#include "materials/Texture.hpp"

#include "sampling/SurfaceScatterEvent.hpp"

#include "math/TangentFrame.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>
#include <memory>

namespace Tungsten
{

class Medium;

class Bsdf : public JsonSerializable
{
protected:
    BsdfLobes _lobes;

    std::shared_ptr<Medium> _intMedium, _extMedium;

    std::shared_ptr<TextureRgb> _base;
    std::shared_ptr<TextureA> _alpha;
    std::shared_ptr<TextureA> _bump;
    float _bumpStrength;

    Vec3f base(const IntersectionInfo *info) const
    {
        return (*_base)[info->uv];
    }

public:
    virtual ~Bsdf()
    {
    }

    Bsdf()
    : _base(std::make_shared<ConstantTextureRgb>(Vec3f(1.0f))),
      _alpha(std::make_shared<ConstantTextureA>(1.0f)),
      _bump(std::make_shared<ConstantTextureA>(0.0f)),
      _bumpStrength(10.0f)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    void setupTangentFrame(const Primitive &primitive, const Primitive::IntersectionTemporary &data,
            const IntersectionInfo &info, TangentFrame &dst) const
    {
        if (_bump->isConstant() && !_lobes.isAnisotropic()) {
            dst = TangentFrame(info.Ns);
            return;
        }
        Vec3f T, B, N(info.Ns);
        if (!primitive.tangentSpace(data, info, T, B)) {
            dst = TangentFrame(info.Ns);
            return;
        }
        if (!_bump->isConstant()) {
            Vec2f dudv;
            _bump->derivatives(info.uv, dudv);

            T += info.Ns*dudv.x()*_bumpStrength;
            B += info.Ns*dudv.y()*_bumpStrength;
            N = T.cross(B);
            if (N.dot(info.Ns) < 0.0f)
                N = -N;
            N.normalize();
        }
        T = (T - N.dot(T)*N).normalized();
        B = (B - N.dot(B)*N - T.dot(B)*T).normalized();

        dst = TangentFrame(N, T, B);
    }

    virtual float alpha(const IntersectionInfo *info) const
    {
        return (*_alpha)[info->uv];
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
        _base = c;
    }

    void setAlpha(const std::shared_ptr<TextureA> &a)
    {
        _alpha = a;
    }

    void setBump(const std::shared_ptr<TextureA> &b)
    {
        _bump = b;
    }

    std::shared_ptr<TextureRgb> &color()
    {
        return _base;
    }

    std::shared_ptr<TextureA> &alpha()
    {
        return _alpha;
    }

    std::shared_ptr<TextureA> &bump()
    {
        return _bump;
    }

    const std::shared_ptr<TextureRgb> &color() const
    {
        return _base;
    }

    const std::shared_ptr<TextureA> &alpha() const
    {
        return _alpha;
    }

    const std::shared_ptr<TextureA> &bump() const
    {
        return _bump;
    }

    const std::shared_ptr<Medium> &extMedium() const
    {
        return _extMedium;
    }

    const std::shared_ptr<Medium> &intMedium() const
    {
        return _intMedium;
    }
};

}

#endif /* BSDF_HPP_ */
