#ifndef RAY_HPP_
#define RAY_HPP_

#include "MathUtil.hpp"
#include "Angle.hpp"
#include "Vec.hpp"

namespace Tungsten {

class Ray
{
    Vec3f _pos;
    Vec3f _dir;
    float _nearT;
    float _farT;
    float _time;
    float _footprint;
    float _diameter;
    bool _primaryRay;

public:
    Ray() = default;

    Ray(const Vec3f &pos, const Vec3f &dir, float nearT = 1e-4f, float farT = 1e30f, float time = 0.0f)
    : _pos(pos), _dir(dir), _nearT(nearT), _farT(farT), _time(time), _footprint(0.0f), _diameter(0.0f), _primaryRay(false)
    {
    }

    Ray scatter(const Vec3f &newPos, const Vec3f &newDir, float newNearT, float pdf) const
    {
        Ray ray(*this);
        ray._pos = newPos;
        ray._dir = newDir;
        ray._nearT = newNearT;
        ray._farT = 1e30f;
        if (pdf > 0.0f) {
            // Clamp solid angle to some arbitrary upper bound to remain conservative
            float solidAngle = min(1.0f/pdf, PI);
            float cosHalfApex = 1.0f - INV_TWO_PI*solidAngle;
            float sinHalfApex = std::sqrt(max(1.0f - sqr(cosHalfApex), 0.0f));
            ray._diameter = max(ray._diameter, 2.0f*sinHalfApex);
        }
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

    float footprint() const
    {
        return _footprint;
    }

    void setFootprint(float footprint)
    {
        _footprint = footprint;
    }

    float diameter() const
    {
        return _diameter;
    }

    void setDiameter(float diameter)
    {
        _diameter = diameter;
    }

    bool isPrimaryRay() const
    {
        return _primaryRay;
    }

    void setPrimaryRay(bool value)
    {
        _primaryRay = value;
    }

    void advanceFootprint()
    {
        _footprint += _farT*_diameter;
    }
};

}

#endif /* RAY_HPP_ */
