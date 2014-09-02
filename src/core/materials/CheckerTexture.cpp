#include "CheckerTexture.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

CheckerTexture::CheckerTexture()
: _onColor(0.8f), _offColor(0.2f),
  _resU(20), _resV(20)
{
}

void CheckerTexture::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Texture::fromJson(v, scene);
    scalarOrVecFromJson(v, "onColor",   _onColor);
    scalarOrVecFromJson(v, "offColor", _offColor);
    JsonUtils::fromJson(v, "resU", _resU);
    JsonUtils::fromJson(v, "resV", _resV);
}

rapidjson::Value CheckerTexture::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Texture::toJson(allocator);
    v.AddMember("type", "checker", allocator);
    v.AddMember("onColor",  scalarOrVecToJson( _onColor, allocator), allocator);
    v.AddMember("offColor", scalarOrVecToJson(_offColor, allocator), allocator);
    v.AddMember("resU", _resU, allocator);
    v.AddMember("resV", _resV, allocator);
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
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)));
    bool on = (uvI.x() ^ uvI.y()) & 1;
    return on ? _onColor : _offColor;
}

void CheckerTexture::derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const
{
    derivs = Vec2f(0.0f);
}

void CheckerTexture::makeSamplable(TextureMapJacobian /*jacobian*/)
{
}

/* TODO this is biased for odd resolutions */
Vec2f CheckerTexture::sample(const Vec2f &uv) const
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

float CheckerTexture::pdf(const Vec2f &uv) const
{
    float  onWeight =  _onColor.max();
    float offWeight = _offColor.max();
    if (onWeight + offWeight == 0.0f)
        return 1.0f;
    Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)));
    bool on = (uvI.x() ^ uvI.y()) & 1;
    return (on ? onWeight : offWeight)/(onWeight + offWeight);
}

}
