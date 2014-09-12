#ifndef DISKTEXTURE_HPP_
#define DISKTEXTURE_HPP_

#include "Texture.hpp"

#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"

namespace Tungsten {

template<bool Scalar>
class DiskTexture : public Texture<Scalar, 2>
{
    typedef JsonSerializable::Allocator Allocator;
    typedef typename Texture<Scalar, 2>::Value Value;

public:
    DiskTexture()
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Texture<Scalar, 2>::toJson(allocator);
        v.AddMember("type", "disk", allocator);
        return std::move(v);
    }

    void derivatives(const Vec<float, 2> &/*uv*/, Vec<Value, 2> &derivs) const override final
    {
        derivs = Vec<Value, 2>(Value(0.0f));
    }

    bool isConstant() const override final
    {
        return false;
    }

    Value average() const override final
    {
        return Value(PI*0.25f);
    }

    Value minimum() const override final
    {
        return Value(0.0f);
    }

    Value maximum() const override final
    {
        return Value(1.0f);
    }

    Value operator[](const Vec<float, 2> &uv) const override final
    {
        return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? Value(1.0f) : Value(0.0f);
    }

    void makeSamplable(TextureMapJacobian /*jacobian*/) override final
    {
    }

    Vec2f sample(const Vec2f &uv) const override final
    {
        return Sample::uniformDisk(uv).xy()*0.5f + 0.5f;
    }

    float pdf(const Vec2f &uv) const override final
    {
        return (uv - Vec2f(0.5f)).lengthSq() < 0.25f ? Sample::uniformDiskPdf()*4.0f : 0.0f;
    }
};

}

#endif /* DISKTEXTURE_HPP_ */
