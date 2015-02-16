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

PinholeCamera::PinholeCamera(const Mat4f &transform, const Vec2u &res, float fov)
: Camera(transform, res),
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
    float weight;
    Vec2f uv = _filter.sample(sampler.next2D(), weight);
    Vec3f dir = _transform.transformVector(Vec3f(
        -1.0f  + (float(pixel.x()) + 0.5f + uv.x())*_pixelSize.x(),
        _ratio - (float(pixel.y()) + 0.5f + uv.y())*_pixelSize.y(),
        _planeDist
    )).normalized();

    throughput = Vec3f(weight);
    ray = Ray(pos(), dir);
    ray.setDiameter(_pixelSize.x()/_planeDist);
    return true;
}

Mat4f PinholeCamera::approximateProjectionMatrix(int width, int height) const
{
    return Mat4f::perspective(_fovDeg, float(width)/float(height), 1e-2f, 100.0f);
}

}
