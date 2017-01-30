#include "BladeTexture.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

BladeTexture::BladeTexture()
: _numBlades(6),
  _angle(0.5f*PI/_numBlades),
  _value(1.0f)
{
    init();
}

void BladeTexture::init()
{
    _bladeAngle = TWO_PI/_numBlades;
    float sinAngle = std::sin(_bladeAngle*0.5f);
    float cosAngle = std::cos(_bladeAngle*0.5f);

    _area = 0.25f*0.5f*_numBlades*std::sin(_bladeAngle);
    _baseEdge = Vec2f(-sinAngle, cosAngle)*2.0f*std::sin(PI/_numBlades);
    _baseNormal = Vec2f(cosAngle, sinAngle);
}

void BladeTexture::fromJson(JsonPtr value, const Scene &scene)
{
    Texture::fromJson(value, scene);
    value.getField("blades", _numBlades);
    value.getField("angle", _angle);
    value.getField("value", _value);

    init();
}

rapidjson::Value BladeTexture::toJson(Allocator &allocator) const
{
    return JsonObject{Texture::toJson(allocator), allocator,
        "type", "blade",
        "blades", _numBlades,
        "angle", _angle,
        "value",  scalarOrVecToJson( _value, allocator)
    };
}

bool BladeTexture::isConstant() const
{
    return false;
}

Vec3f BladeTexture::average() const
{
    return _area*_value;
}

Vec3f BladeTexture::minimum() const
{
    return Vec3f(0.0f);
}

Vec3f BladeTexture::maximum() const
{
    return _value;
}

Vec3f BladeTexture::operator[](const Vec2f &uv) const
{
    if (uv.sum() == 0.0f)
        return _value;

    Vec2f globalUv = uv*2.0f - 1.0f;
    float phi = std::atan2(globalUv.y(), globalUv.x()) - _angle;
    phi = -(std::floor(phi/_bladeAngle)*_bladeAngle + _angle);
    float sinPhi = std::sin(phi);
    float cosPhi = std::cos(phi);
    Vec2f localUv(globalUv.x()*cosPhi - globalUv.y()*sinPhi, globalUv.y()*cosPhi + globalUv.x()*sinPhi);
    if (_baseNormal.dot(localUv - Vec2f(1.0f, 0.0f)) > 0.0f)
        return Vec3f(0.0f);
    return _value;
}

Vec3f BladeTexture::operator[](const IntersectionInfo &info) const
{
    return (*this)[info.uv];
}

void BladeTexture::derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const
{
    derivs = Vec2f(0.0f);
}

void BladeTexture::makeSamplable(TextureMapJacobian /*jacobian*/)
{
}

Vec2f BladeTexture::sample(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
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

float BladeTexture::pdf(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
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

void BladeTexture::scaleValues(float factor)
{
    _value *= factor;
}

Texture *BladeTexture::clone() const
{
    return new BladeTexture(*this);
}

void BladeTexture::setAngle(float angle)
{
    _angle = angle;
    init();
}

void BladeTexture::setNumBlades(int numBlades)
{
    _numBlades = numBlades;
    init();
}

}
