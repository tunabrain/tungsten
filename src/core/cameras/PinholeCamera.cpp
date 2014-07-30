#include "PinholeCamera.hpp"

#include "sampling/SampleGenerator.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/JsonUtils.hpp"

#include <cmath>

namespace Tungsten {

PinholeCamera::PinholeCamera()
: Camera(),
  _fovDeg(60.0f)
{
    precompute();
}

PinholeCamera::PinholeCamera(const Mat4f &transform, const Vec2u &res, float fov, uint32 spp)
: Camera(transform, res, spp),
  _fovDeg(fov)
{
    precompute();
}

void PinholeCamera::precompute()
{
    _fovRad = Angle::degToRad(_fovDeg);
    _planeDist = 1.0f/std::tan(_fovRad*0.5f);
}

void PinholeCamera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Camera::fromJson(v, scene);
    JsonUtils::fromJson(v, "fov", _fovDeg);

    precompute();
}

rapidjson::Value PinholeCamera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Camera::toJson(allocator);
    v.AddMember("type", "pinhole", allocator);
    v.AddMember("fov", _fovDeg, allocator);
    return std::move(v);
}

bool PinholeCamera::generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const
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

Mat4f PinholeCamera::approximateProjectionMatrix(int width, int height) const
{
    return Mat4f::perspective(_fovDeg, float(width)/float(height), 1e-2f, 100.0f);
}

}
