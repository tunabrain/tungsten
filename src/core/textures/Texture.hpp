#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten {

struct IntersectionInfo;

enum TextureMapJacobian {
    MAP_UNIFORM,
    MAP_SPHERICAL,
    MAP_JACOBIAN_COUNT,
};

class Texture : public JsonSerializable
{
protected:
    static rapidjson::Value scalarOrVecToJson(const Vec3f &src, Allocator &allocator);

public:
    virtual ~Texture() {}

    virtual bool isConstant() const = 0;

    virtual Vec3f average() const = 0;
    virtual Vec3f minimum() const = 0;
    virtual Vec3f maximum() const = 0;

    virtual Vec3f operator[](const Vec2f &uv) const = 0;
    virtual Vec3f operator[](const IntersectionInfo &info) const = 0;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const = 0;

    virtual void makeSamplable(TextureMapJacobian jacobian) = 0;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const = 0;
    virtual Vec2f invert(TextureMapJacobian jacobian, const Vec2f &uv) const;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const = 0;

    virtual void scaleValues(float factor) = 0;

    virtual Texture *clone() const = 0;
};

}

#endif /* TEXTURE_HPP_ */
