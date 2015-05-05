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

bool PinholeCamera::sampleInboundDirection(LensSample &sample) const
{
    sample.d = _pos - sample.p;

    Vec3f localD = _invTransform.transformVector(-sample.d);
    if (localD.z() <= 0.0f)
        return false;
    localD *= _planeDist/localD.z();

    float weight, pdf;
    Vec2f uv = _filter.sample(sampler.next2D(), weight, pdf);
    float pixelX = (localD.x() + 1.0f)/_pixelSize.x() - uv.x();
    float pixelY = (_ratio - localD.y())/_pixelSize.y() - uv.y();
    if (pixelX < 0.0f || pixelY < 0.0f)
        return false;

    sample.pixel.x() = uint32(pixelX);
    sample.pixel.y() = uint32(pixelY);
    if (sample.pixel.x() >= _res.x() || sample.pixel.y() >= _res.y())
        return false;


    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;

    weight *= sqr(_planeDist)/(_pixelSize.x()*_pixelSize.y()*rSq*cube(localD.z()/localD.length()));

    sample.pdf = 1.0f;
    sample.weight = Vec3f(weight);
    return true;
}

bool PinholeCamera::generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const
{
    float weight, pdf;
    Vec2f uv = _filter.sample(sampler.next2D(), weight, pdf);
    Vec3f dir = _transform.transformVector(Vec3f(
        -1.0f  + (float(pixel.x()) + 0.5f + uv.x())*_pixelSize.x(),
        _ratio - (float(pixel.y()) + 0.5f + uv.y())*_pixelSize.y(),
        _planeDist
    )).normalized();

    throughput = Vec3f(weight);
    ray = Ray(pos(), dir);
    return true;
}

Mat4f PinholeCamera::approximateProjectionMatrix(int width, int height) const
{
    return Mat4f::perspective(_fovDeg, float(width)/float(height), 1e-2f, 100.0f);
}

}
