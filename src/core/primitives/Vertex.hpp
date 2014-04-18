#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include "math/Vec.hpp"

namespace Tungsten
{

class Vertex
{
    Vec3f _pos, _normal;
    Vec2f _uv;

public:
    Vertex() = default;

    Vertex(const Vec3f &pos)
    : _pos(pos)
    {
    }

    Vertex(const Vec3f &pos, const Vec3f &normal, const Vec2f &uv)
    : _pos(pos), _normal(normal), _uv(uv)
    {
    }

    const Vec3f &normal() const
    {
        return _normal;
    }

    const Vec3f &pos() const
    {
        return _pos;
    }

    const Vec2f &uv() const
    {
        return _uv;
    }

    Vec3f &normal()
    {
        return _normal;
    }

    Vec3f &pos()
    {
        return _pos;
    }

    Vec2f &uv()
    {
        return _uv;
    }
};

}



#endif /* VERTEX_HPP_ */
