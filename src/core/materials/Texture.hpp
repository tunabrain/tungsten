#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <type_traits>

namespace Tungsten
{

template<bool Scalar, int Dimension>
class Texture : public JsonSerializable
{
public:
    typedef typename std::conditional<Scalar, float, Vec3f>::type Value;

    virtual ~Texture() {}

    virtual bool isConstant() const = 0;

    virtual Value average() const = 0;
    virtual Value minimum() const = 0;
    virtual Value maximum() const = 0;

    virtual void derivatives(const Vec<float, Dimension> &uv, Vec<Value, Dimension> &derivs) const = 0;

    virtual Value operator[](const Vec<float, Dimension> &uv) const = 0;
};

template<bool Scalar, int Dimension>
class ConstantTexture : public Texture<Scalar, Dimension>
{
    typedef JsonSerializable::Allocator Allocator;
    typedef typename Texture<Scalar, Dimension>::Value Value;

    Value _value;

public:
    ConstantTexture()
    : _value(0.0f)
    {
    }

    ConstantTexture(Value value)
    : _value(value)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &/*scene*/) override final
    {
        JsonUtils::fromJson(v, "value", _value);
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        return JsonUtils::toJsonValue(_value, allocator);
    }

    void derivatives(const Vec<float, Dimension> &/*uv*/, Vec<Value, Dimension> &derivs) const override final
    {
        derivs = Vec<Value, Dimension>(Value(0.0f));
    }

    bool isConstant() const override final
    {
        return true;
    }

    Value average() const override final
    {
        return _value;
    }

    Value minimum() const override final
    {
        return _value;
    }

    Value maximum() const override final
    {
        return _value;
    }

    Value operator[](const Vec<float, Dimension> &/*uv*/) const override final
    {
        return _value;
    }
};

typedef Texture<true, 2> TextureA;
typedef Texture<false, 2> TextureRgb;
typedef ConstantTexture<true, 2> ConstantTextureA;
typedef ConstantTexture<false, 2> ConstantTextureRgb;

}

#endif /* TEXTURE_HPP_ */
