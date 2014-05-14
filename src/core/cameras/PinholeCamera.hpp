#ifndef PINHOLECAMERA_HPP_
#define PINHOLECAMERA_HPP_

#include <cmath>

#include "math/Angle.hpp"

#include "Camera.hpp"

namespace Tungsten
{

class Scene;

class PinholeCamera : public Camera
{
    float _fovDeg;
    float _fovRad;
    float _planeDist;

    void precompute()
    {
        _fovRad = Angle::degToRad(_fovDeg);
        _planeDist = 1.0f/std::tan(_fovRad*0.5f);
    }

public:
    PinholeCamera()
    : Camera(),
      _fovDeg(60.0f)
    {
        precompute();
    }

    PinholeCamera(const Mat4f &transform, const Vec2u &res, float fov, uint32 spp)
    : Camera(transform, res, spp),
      _fovDeg(fov)
    {
        precompute();
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Camera::fromJson(v, scene);
        JsonUtils::fromJson(v, "fov", _fovDeg);

        precompute();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Camera::toJson(allocator);
        v.AddMember("type", "pinhole", allocator);
        v.AddMember("fov", _fovDeg, allocator);
        return std::move(v);
    }

    bool generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const override final
    {
        Vec2f uv = sampler.next2D();
        Vec3f dir = _transform.transformVector(Vec3f(
            -1.0f  + (float(pixel.x()) + uv.x())*_pixelSize.x(),
            _ratio - (float(pixel.y()) + uv.y())*_pixelSize.y(),
            _planeDist
        )).normalized();

        throughput = Vec3f(1.0f);
        ray = Ray(pos(), dir);
        return true;
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
};

}

#endif /* PINHOLECAMERA_HPP_ */
