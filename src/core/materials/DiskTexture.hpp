#ifndef DISKTEXTURE_HPP_
#define DISKTEXTURE_HPP_

#include "Texture.hpp"

#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"

namespace Tungsten {

class DiskTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

public:
    DiskTexture()
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Texture::toJson(allocator);
        v.AddMember("type", "disk", allocator);
        return std::move(v);
    }

    void derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const override final
    {
        derivs = Vec2f(0.0f);
    }

    bool isConstant() const override final
    {
        return false;
    }

    Vec3f average() const override final
    {
        return Vec3f(PI*0.25f);
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
        return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? Vec3f(1.0f) : Vec3f(0.0f);
    }

    void makeSamplable(TextureMapJacobian /*jacobian*/) override final
    {
    }

    Vec2f sample(const Vec2f &uv) const override final
    {
        return SampleWarp::uniformDisk(uv).xy()*0.5f + 0.5f;
    }

    float pdf(const Vec2f &uv) const override final
    {
        return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? SampleWarp::uniformDiskPdf()*4.0f : 0.0f;
    }
};

}

#endif /* DISKTEXTURE_HPP_ */
