#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <type_traits>

namespace Tungsten {

enum TextureMapJacobian {
    MAP_UNIFORM,
    MAP_SPHERICAL,
};

class Texture : public JsonSerializable
{
protected:
    static inline bool scalarOrVecFromJson(const rapidjson::Value &v, const char *field, Vec3f &dst)
    {
        float scalar;
        if (JsonUtils::fromJson(v, field, scalar)) {
            dst = Vec3f(scalar);
            return true;
        } else {
            return JsonUtils::fromJson(v, field, dst);
        }
    }

    static inline rapidjson::Value scalarOrVecToJson(const Vec3f &src, Allocator &allocator)
    {
        if (src.x() == src.y() && src.y() == src.z())
            return std::move(JsonUtils::toJsonValue(src.x(), allocator));
        else
            return std::move(JsonUtils::toJsonValue(src, allocator));
    }

public:
    virtual ~Texture() {}

    virtual bool isConstant() const = 0;

    virtual Vec3f average() const = 0;
    virtual Vec3f minimum() const = 0;
    virtual Vec3f maximum() const = 0;

    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const = 0;

    virtual Vec3f operator[](const Vec2f &uv) const = 0;

    virtual void makeSamplable(TextureMapJacobian jacobian) = 0;
    virtual Vec2f sample(const Vec2f &uv) const = 0;
    virtual float pdf(const Vec2f &uv) const = 0;
};

class ConstantTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    Vec3f _value;

public:
    ConstantTexture()
    : _value(0.0f)
    {
    }

    ConstantTexture(float value)
    : _value(value)
    {
    }

    ConstantTexture(const Vec3f &value)
    : _value(value)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &/*scene*/) override final
    {
        scalarOrVecFromJson(v, "value", _value);
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        return std::move(scalarOrVecToJson(_value, allocator));
    }

    void derivatives(const Vec2f &/*uv*/, Vec2f &derivs) const override final
    {
        derivs = Vec2f(0.0f);
    }

    bool isConstant() const override final
    {
        return true;
    }

    Vec3f average() const override final
    {
        return _value;
    }

    Vec3f minimum() const override final
    {
        return _value;
    }

    Vec3f maximum() const override final
    {
        return _value;
    }

    Vec3f operator[](const Vec2f &/*uv*/) const override final
    {
        return _value;
    }

    void makeSamplable(TextureMapJacobian /*jacobian*/) override final
    {
    }

    Vec2f sample(const Vec2f &uv) const override final
    {
        return uv;
    }

    float pdf(const Vec2f &/*uv*/) const override final
    {
        return 1.0f;
    }
};

}

#endif /* TEXTURE_HPP_ */
