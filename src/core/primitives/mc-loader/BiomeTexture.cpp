#include "BiomeTexture.hpp"
#include "TraceableMinecraftMap.hpp"
#include "ResourcePackLoader.hpp"

namespace Tungsten {
namespace MinecraftLoader {

void BiomeTexture::fromJson(JsonPtr /*value*/, const Scene &/*scene*/)
{
}

rapidjson::Value BiomeTexture::toJson(Allocator &allocator) const
{
    return Texture::toJson(allocator);
}

bool BiomeTexture::isConstant() const
{
    return false;
}

// Not really useful return values, but not used at the moment
Vec3f BiomeTexture::average() const
{
    return Vec3f(0.5f);
}
Vec3f BiomeTexture::minimum() const
{
    return Vec3f(0.0f);
}
Vec3f BiomeTexture::maximum() const
{
    return Vec3f(1.0f);
}

Vec3f BiomeTexture::operator[](const Vec2f &/*uv*/) const
{
    return Vec3f(0.0f);
}

Vec3f BiomeTexture::operator[](const IntersectionInfo &info) const
{
    Vec3f overlay = _overlay ? (*_overlay)[info.uv] : Vec3f(0.0f);
    Vec3f substrate = _substrate ? (*_substrate)[info.uv] : Vec3f(0.0f);
    float alpha = _overlayOpacity ? (*_overlayOpacity)[info.uv].x() : 1.0f;

    if (_tintType == ResourcePackLoader::TINT_NONE)
        return lerp(substrate, overlay, alpha);

    Vec2i pi(Vec2f(info.p.x(), info.p.z()));
    Vec2i key = pi >> 8;
    pi -= (key << 8);

    Vec2f pf = Vec2f(pi)*(1.0f/256.0f);

    auto iter = _biomes.find(key);
    if (iter == _biomes.end())
        return overlay;

    Vec3f bottom, top;
    if (_tintType == ResourcePackLoader::TINT_FOLIAGE) {
        bottom = (*iter->second->foliageBottom)[pf];
        top    = (*iter->second->foliageTop)   [pf];
    } else {
        bottom = (*iter->second->grassBottom)[pf];
        top    = (*iter->second->grassTop)   [pf];
    }
    float height = iter->second->heights[pi.x() + 256*pi.y()];

    float t = clamp((info.p.y() - 64.0f)/height, 0.0f, 1.0f);

    return lerp(substrate, lerp(bottom, top, t)*overlay, alpha);
}

void BiomeTexture::derivatives(const Vec2f &uv, Vec2f &derivs) const
{
    return _substrate->derivatives(uv, derivs);
}

void BiomeTexture::makeSamplable(TextureMapJacobian jacobian)
{
    _substrate->makeSamplable(jacobian);
}

Vec2f BiomeTexture::sample(TextureMapJacobian jacobian, const Vec2f &uv) const
{
    return _substrate->sample(jacobian, uv);
}

float BiomeTexture::pdf(TextureMapJacobian jacobian, const Vec2f &uv) const
{
    return _substrate->pdf(jacobian, uv);
}

void BiomeTexture::scaleValues(float factor)
{
    if (_substrate)
        _substrate->scaleValues(factor);
    if (_overlay)
        _overlay->scaleValues(factor);
}

Texture *BiomeTexture::clone() const
{
    return new BiomeTexture(*this);
}

}
}
