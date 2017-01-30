#include "DiskTexture.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

DiskTexture::DiskTexture()
: _value(1.0f)
{
}

void DiskTexture::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    value.getField("value", _value);
}

rapidjson::Value DiskTexture::toJson(Allocator &allocator) const
{
    return JsonObject{Texture::toJson(allocator), allocator,
        "type", "disk",
        "value",  scalarOrVecToJson( _value, allocator)
    };
}

bool DiskTexture::isConstant() const
{
    return false;
}

Vec3f DiskTexture::average() const
{
    return PI*0.25f*_value;
}

Vec3f DiskTexture::minimum() const
{
    return Vec3f(0.0f);
}

Vec3f DiskTexture::maximum() const
{
    return _value;
}

Vec3f DiskTexture::operator[](const Vec<float, 2> &uv) const
{
    return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? _value : Vec3f(0.0f);
}

Vec3f DiskTexture::operator[](const IntersectionInfo &info) const
{
    return (*this)[info.uv];
}

void DiskTexture::derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const
{
    derivs = Vec2f(0.0f);
}

void DiskTexture::makeSamplable(TextureMapJacobian /*jacobian*/)
{
}

Vec2f DiskTexture::sample(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
{
    return SampleWarp::uniformDisk(uv).xy()*0.5f + 0.5f;
}

float DiskTexture::pdf(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
{
    return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? SampleWarp::uniformDiskPdf()*4.0f : 0.0f;
}

void DiskTexture::scaleValues(float factor)
{
    _value *= factor;
}

Texture *DiskTexture::clone() const
{
    return new DiskTexture(*this);
}

}
