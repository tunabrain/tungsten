#include "ConstantTexture.hpp"

namespace Tungsten {

ConstantTexture::ConstantTexture()
: _value(0.0f)
{
}

ConstantTexture::ConstantTexture(float value)
: _value(value)
{
}

ConstantTexture::ConstantTexture(const Vec3f &value)
: _value(value)
{
}

void ConstantTexture::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    scalarOrVecFromJson(v, "value", _value);
}

rapidjson::Value ConstantTexture::toJson(Allocator &allocator) const
{
    return std::move(scalarOrVecToJson(_value, allocator));
}

bool ConstantTexture::isConstant() const
{
    return true;
}

Vec3f ConstantTexture::average() const
{
    return _value;
}

Vec3f ConstantTexture::minimum() const
{
    return _value;
}

Vec3f ConstantTexture::maximum() const
{
    return _value;
}

Vec3f ConstantTexture::operator[](const Vec2f &/*uv*/) const
{
    return _value;
}

void ConstantTexture::derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const
{
    derivs = Vec2f(0.0f);
}

void ConstantTexture::makeSamplable(TextureMapJacobian /*jacobian*/)
{
}

Vec2f ConstantTexture::sample(const Vec2f &uv) const
{
    return uv;
}

float ConstantTexture::pdf(const Vec2f &/*uv*/) const
{
    return 1.0f;
}

}
