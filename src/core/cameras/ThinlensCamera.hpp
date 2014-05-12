#ifndef THINLENSCAMERA_HPP_
#define THINLENSCAMERA_HPP_

#include <cmath>

#include "math/Angle.hpp"

#include "Camera.hpp"

namespace Tungsten
{

class Scene;

class ThinlensCamera : public Camera
{
    float _fovDeg;
    float _fovRad;
    float _planeDist;
    float _focusDist;
    float _apertureSize;

    void precompute()
    {
        _fovRad = Angle::degToRad(_fovDeg);
        _planeDist = 1.0f/std::tan(_fovRad*0.5f);
    }

public:
    ThinlensCamera()
    : Camera(),
      _fovDeg(60.0f),
      _focusDist(1.0f),
      _apertureSize(0.001f)
    {
        precompute();
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Camera::fromJson(v, scene);
        JsonUtils::fromJson(v, "fov", _fovDeg);
        JsonUtils::fromJson(v, "focusDistance", _focusDist);
        JsonUtils::fromJson(v, "apertureSize", _apertureSize);

        precompute();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Camera::toJson(allocator);
        v.AddMember("type", "thinlens", allocator);
        v.AddMember("fov", _fovDeg, allocator);
        v.AddMember("focusDistance", _focusDist, allocator);
        v.AddMember("apertureSize", _apertureSize, allocator);
        return std::move(v);
    }

    Ray generateSample(Vec2u pixel, SampleGenerator &sampler) const override final
    {
        Vec2f pixelUv = sampler.next2D();
        Vec2f lensUv = sampler.next2D();
        Vec3f planePos(
            -1.0f  + (float(pixel.x()) + pixelUv.x())*_pixelSize.x(),
            _ratio - (float(pixel.y()) + pixelUv.y())*_pixelSize.y(),
            _planeDist
        );
        planePos *= _focusDist/planePos.z();
        Vec3f lensPos = Sample::uniformDisk(lensUv)*_apertureSize;
        Vec3f dir = _transform.transformVector(planePos - lensPos).normalized();
        return Ray(_transform*lensPos, dir);
    }

    Mat4f approximateProjectionMatrix(int width, int height) const override final
    {
        return Mat4f::perspective(_fovDeg, float(width)/float(height), 1e-2f, 100.0f);
    }

    float approximateFov() const override final
    {
        return _fovRad;
    }

    float fovDeg() const
    {
        return _fovDeg;
    }

    float apertureSize() const
    {
        return _apertureSize;
    }

    float focusDist() const
    {
        return _focusDist;
    }
};

}

#endif /* THINLENSCAMERA_HPP_ */
