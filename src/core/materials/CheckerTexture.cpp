#include "CheckerTexture.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

CheckerTexture::CheckerTexture()
: _onColor(0.8f), _offColor(0.2f),
  _resU(20), _resV(20),
  _offsetU(0.5f), _offsetV(0.5f)
{
}

CheckerTexture::CheckerTexture(Vec3f onColor, Vec3f offColor, int resU, int resV, float offsetU, float offsetV)
: _onColor(onColor), _offColor(offColor),
  _resU(resU), _resV(resV),
  _offsetU(offsetU), _offsetV(offsetV)
{
}

void CheckerTexture::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Texture::fromJson(v, scene);
    scalarOrVecFromJson(v, "on_color",   _onColor);
    scalarOrVecFromJson(v, "off_color", _offColor);
    JsonUtils::fromJson(v, "res_u", _resU);
    JsonUtils::fromJson(v, "res_v", _resV);
    JsonUtils::fromJson(v, "offset_u", _offsetU);
    JsonUtils::fromJson(v, "offset_v", _offsetV);
}

rapidjson::Value CheckerTexture::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Texture::toJson(allocator);
    v.AddMember("type", "checker", allocator);
    v.AddMember("on_color",  scalarOrVecToJson( _onColor, allocator), allocator);
    v.AddMember("off_color", scalarOrVecToJson(_offColor, allocator), allocator);
    v.AddMember("res_u", _resU, allocator);
    v.AddMember("res_v", _resV, allocator);
    v.AddMember("offset_u", _offsetU, allocator);
    v.AddMember("offset_v", _offsetV, allocator);
    return std::move(v);
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
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)) + Vec2f(_offsetU, _offsetV));
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
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV))+ Vec2f(_offsetU, _offsetV));
    bool on = (uvI.x() ^ uvI.y()) & 1;
    return (on ? onWeight : offWeight)/(onWeight + offWeight);
}

}
