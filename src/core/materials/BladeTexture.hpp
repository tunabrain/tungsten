#ifndef BLADETEXTURE_HPP_
#define BLADETEXTURE_HPP_

#include "Texture.hpp"

#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

namespace Tungsten {

class BladeTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    int _numBlades;
    float _angle;

    float _area;
    float _bladeAngle;
    Vec2f _baseNormal;
    Vec2f _baseEdge;

    void init()
    {
        _bladeAngle = TWO_PI/_numBlades;
        float sinAngle = std::sin(_bladeAngle*0.5f);
        float cosAngle = std::cos(_bladeAngle*0.5f);

        _area = 0.25f*0.5f*_numBlades*std::sin(_bladeAngle);
        _baseEdge = Vec2f(-sinAngle, cosAngle)*2.0f*std::sin(PI/_numBlades);
        _baseNormal = Vec2f(cosAngle, sinAngle);
    }

public:
    BladeTexture()
    : _numBlades(6),
      _angle(0.5f*PI/_numBlades)
    {
        init();
    }


    void fromJson(const rapidjson::Value &v, const Scene &scene) override final
    {
        Texture::fromJson(v, scene);
        JsonUtils::fromJson(v, "blades", _numBlades);
        JsonUtils::fromJson(v, "angle", _angle);

        init();
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Texture::toJson(allocator);
        v.AddMember("type", "blade", allocator);
        v.AddMember("blades", _numBlades, allocator);
        v.AddMember("angle", _angle, allocator);
        return std::move(v);
    }

    void derivatives(const Vec<float, 2> &/*uv*/, Vec2f &derivs) const override final
    {
        derivs = Vec2f(0.0f);
    }

    bool isConstant() const override final
    {
        return false;
    }

    Vec3f average() const override final
    {
        return Vec3f(_area);
    }

    Vec3f minimum() const override final
    {
        return Vec3f(0.0f);
    }

    Vec3f maximum() const override final
    {
        return Vec3f(1.0f);
    }

    Vec3f operator[](const Vec<float, 2> &uv) const override final
    {
        if (uv.sum() == 0.0f)
            return Vec3f(1.0f);
        Vec2f globalUv = uv*2.0f - 1.0f;
        float phi = std::atan2(globalUv.y(), globalUv.x()) - _angle;
        phi = -(std::floor(phi/_bladeAngle)*_bladeAngle + _angle);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);
        Vec2f localUv(globalUv.x()*cosPhi - globalUv.y()*sinPhi, globalUv.y()*cosPhi + globalUv.x()*sinPhi);
        if (_baseNormal.dot(localUv - Vec2f(1.0f, 0.0f)) > 0.0f)
            return Vec3f(0.0f);
        return Vec3f(1.0f);
    }

    void makeSamplable(TextureMapJacobian /*jacobian*/) override final
    {
    }

    Vec2f sample(const Vec2f &uv) const override final
    {
        float u = uv.x()*_numBlades;
        int blade = int(u);
        u -= blade;

        float phi = _angle + blade*_bladeAngle;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        float uSqrt = std::sqrt(u);
        float alpha = 1.0f - uSqrt;
        float beta = (1.0f - uv.y())*uSqrt;

        Vec2f localUv((1.0f + _baseEdge.x())*beta + (1.0f - alpha - beta), _baseEdge.y()*beta);

        return Vec2f(
            localUv.x()*cosPhi - localUv.y()*sinPhi,
            localUv.y()*cosPhi + localUv.x()*sinPhi
        )*0.5f + 0.5f;
    }

    float pdf(const Vec2f &uv) const override final
    {
        if (uv.sum() == 0.0f)
            return 1.0f/_area;
        Vec2f globalUv = uv*2.0f - 1.0f;
        float phi = std::atan2(globalUv.y(), globalUv.x()) - _angle;
        phi = -(std::floor(phi/_bladeAngle)*_bladeAngle + _angle);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);
        Vec2f localUv(globalUv.x()*cosPhi - globalUv.y()*sinPhi, globalUv.y()*cosPhi + globalUv.x()*sinPhi);
        if (_baseNormal.dot(localUv - Vec2f(1.0f, 0.0f)) > 0.0f)
            return 0.0f;
        return 1.0f/_area;
    }
};

}

#endif /* BLADETEXTURE_HPP_ */
