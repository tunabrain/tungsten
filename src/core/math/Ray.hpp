#ifndef RAY_HPP_
#define RAY_HPP_

#include "Vec.hpp"

namespace Tungsten {

class Ray
{
    Vec3f _pos;
    Vec3f _dir;
    float _nearT;
    float _farT;
    float _time;

public:
    Ray() = default;

    Ray(const Vec3f &pos, const Vec3f &dir, float nearT = 1e-4f, float farT = 1e30f, float time = 0.0f)
    : _pos(pos), _dir(dir), _nearT(nearT), _farT(farT), _time(time)
    {
    }

    Vec3f hitpoint() const
    {
        return _pos + _dir*_farT;
    }

    const Vec3f& dir() const
    {
        return _dir;
    }

    void setDir(const Vec3f& dir)
    {
        _dir = dir;
    }

    const Vec3f& pos() const
    {
        return _pos;
    }

    void setPos(const Vec3f& pos)
    {
        _pos = pos;
    }

    float farT() const
    {
        return _farT;
    }

    void setFarT(float farT)
    {
        _farT = farT;
    }

    float nearT() const
    {
        return _nearT;
    }

    void setNearT(float nearT)
    {
        _nearT = nearT;
    }

    float time() const
    {
        return _time;
    }

    void setTime(float time)
    {
        _time = time;
    }
};

}

#endif /* RAY_HPP_ */
