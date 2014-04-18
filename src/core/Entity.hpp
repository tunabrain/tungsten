#ifndef ENTITY_HPP_
#define ENTITY_HPP_

#include "io/JsonSerializable.hpp"

namespace Tungsten
{

class TriangleMesh;

class Entity : public JsonSerializable
{
protected:
    Mat4f _transform;

public:
    Entity() = default;

    Entity(const std::string &name)
    : JsonSerializable(name)
    {
    }

    Entity(const rapidjson::Value &v)
    : JsonSerializable(v),
      _transform(JsonUtils::fromJsonMember<Mat4f>(v, "transform"))
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v = JsonSerializable::toJson(allocator);
        v.AddMember("transform", JsonUtils::toJsonValue(_transform, allocator), allocator);
        return std::move(v);
    }

    virtual const TriangleMesh &asTriangleMesh() = 0;
    virtual void prepareForRender() = 0;

    void setTransform(const Mat4f &m)
    {
        _transform = m;
    }

    const Mat4f &transform() const
    {
        return _transform;
    }
};

}

#endif /* ENTITY_HPP_ */
