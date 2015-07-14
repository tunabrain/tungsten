#ifndef RAY_HPP_
#define RAY_HPP_

#include "MathUtil.hpp"
#include "Angle.hpp"
#include "Vec.hpp"

#include <limits>

namespace Tungsten {

class Ray
{
    Vec3f _pos;
    Vec3f _dir;
    float _nearT;
    float _farT;
    float _time;
    bool _primaryRay;

public:
    Ray() = default;

    Ray(const Vec3f &pos, const Vec3f &dir, float nearT = 1e-4f, float farT = infinity(), float time = 0.0f)
    : _pos(pos), _dir(dir), _nearT(nearT), _farT(farT), _time(time), _primaryRay(false)
    {
    }

    Ray scatter(const Vec3f &newPos, const Vec3f &newDir, float newNearT, float newFarT = infinity()) const
    {
        Ray ray(*this);
        ray._pos = newPos;
        ray._dir = newDir;
        ray._nearT = newNearT;
        ray._farT = newFarT;
        return ray;
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

    bool isPrimaryRay() const
    {
        return _primaryRay;
    }

    void setPrimaryRay(bool value)
    {
        _primaryRay = value;
    }

    static inline float infinity()
    {
        return std::numeric_limits<float>::infinity();
    }
};

}

#endif /* RAY_HPP_ */
