#include "CheckerTexture.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

CheckerTexture::CheckerTexture()
: _onColor(0.8f), _offColor(0.2f),
  _resU(20), _resV(20)
{
}

CheckerTexture::CheckerTexture(Vec3f onColor, Vec3f offColor, int resU, int resV)
: _onColor(onColor), _offColor(offColor),
  _resU(resU), _resV(resV)
{
}

void CheckerTexture::fromJson(JsonPtr value, const Scene &scene)
{
    Texture::fromJson(value, scene);
    value.getField("on_color",   _onColor);
    value.getField("off_color", _offColor);
    value.getField("res_u", _resU);
    value.getField("res_v", _resV);
}

rapidjson::Value CheckerTexture::toJson(Allocator &allocator) const
{
    return JsonObject{Texture::toJson(allocator), allocator,
        "type", "checker",
        "on_color",  scalarOrVecToJson( _onColor, allocator),
        "off_color", scalarOrVecToJson(_offColor, allocator),
        "res_u", _resU,
        "res_v", _resV
    };
}

bool CheckerTexture::isConstant() const
{
    return false;
}

/* TODO: This is wrong for odd resolutions */
Vec3f CheckerTexture::average() const
{
    return (_onColor + _offColor)*0.5f;
}

Vec3f CheckerTexture::minimum() const
{
    return min(_onColor, _offColor);
}

Vec3f CheckerTexture::maximum() const
{
    return max(_onColor, _offColor);
}

Vec3f CheckerTexture::operator[](const Vec2f &uv) const
{
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)));
    bool on = (uvI.x() ^ uvI.y()) & 1;
    return on ? _onColor : _offColor;
}

Vec3f CheckerTexture::operator[](const IntersectionInfo &info) const
{
    return (*this)[info.uv];
}

void CheckerTexture::derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const
{
    derivs = Vec2f(0.0f);
}

void CheckerTexture::makeSamplable(TextureMapJacobian /*jacobian*/)
{
}

/* TODO this is biased for odd resolutions */
Vec2f CheckerTexture::sample(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
{
    float  onWeight =  _onColor.max();
    float offWeight = _offColor.max();
    if (onWeight + offWeight == 0.0f)
        return uv;
    float onProb = onWeight/(onWeight + offWeight);

    float u = uv.x();
    float v = uv.y();
    int rowOffset;
    if (u < onProb) {
        u = uv.x()/onProb;
        rowOffset = (int(u*_resU) + 1) & 1;
    } else {
        u = (uv.x() - onProb)/(1.0f - onProb);
        rowOffset = int(u*_resU) & 1;
    }

    int numVCells = (_resV + 1 - rowOffset)/2;
    float scaledV = v*numVCells;
    int onCell = int(scaledV);
    float vBase = (onCell*2 + rowOffset)/float(_resV);
    v = vBase + (scaledV - onCell)/float(_resV);

    return Vec2f(u, v);
}

float CheckerTexture::pdf(TextureMapJacobian /*jacobian*/, const Vec2f &uv) const
{
    float  onWeight =  _onColor.max();
    float offWeight = _offColor.max();
    if (onWeight + offWeight == 0.0f)
        return 1.0f;
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)));
    bool on = (uvI.x() ^ uvI.y()) & 1;
    return (on ? onWeight : offWeight)/(onWeight + offWeight);
}

void CheckerTexture::scaleValues(float factor)
{
    _onColor *= factor;
    _offColor *= factor;
}

Texture *CheckerTexture::clone() const
{
    return new CheckerTexture(*this);
}

}
