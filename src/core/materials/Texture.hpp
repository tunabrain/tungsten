#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten {

enum TextureMapJacobian {
    MAP_UNIFORM,
    MAP_SPHERICAL,
};

class Texture : public JsonSerializable
{
protected:
    static bool scalarOrVecFromJson(const rapidjson::Value &v, const char *field, Vec3f &dst);
    static rapidjson::Value scalarOrVecToJson(const Vec3f &src, Allocator &allocator);

public:
    virtual ~Texture() {}

    virtual bool isConstant() const = 0;

    virtual Vec3f average() const = 0;
    virtual Vec3f minimum() const = 0;
    virtual Vec3f maximum() const = 0;

    virtual Vec3f operator[](const Vec2f &uv) const = 0;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const = 0;

    virtual void makeSamplable(TextureMapJacobian jacobian) = 0;
    virtual Vec2f sample(const Vec2f &uv) const = 0;
    virtual float pdf(const Vec2f &uv) const = 0;
};

}

#endif /* TEXTURE_HPP_ */
