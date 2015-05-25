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

bool PinholeCamera::samplePosition(SampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.weight = Vec3f(1.0f);
    sample.pdf = 1.0f;
    sample.Ng = _transform.fwd();

    return true;
}

bool PinholeCamera::sampleDirection(SampleGenerator &sampler, const PositionSample &point,
        DirectionSample &sample) const
{
    Vec2u pixel(sampler.next2D()*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool PinholeCamera::sampleDirection(SampleGenerator &sampler, const PositionSample &/*point*/, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f uv = _filter.sample(sampler.next2D(), pdf);
    Vec3f localD = Vec3f(
        -1.0f  + (float(pixel.x()) + 0.5f + uv.x())*_pixelSize.x(),
        _ratio - (float(pixel.y()) + 0.5f + uv.y())*_pixelSize.y(),
        _planeDist
    ).normalized();

    sample.d =  _transform.transformVector(localD);
    sample.weight = Vec3f(1.0f);
    sample.pdf = (_planeDist*2.0f)*(_planeDist*_ratio*2.0f)/cube(localD.z());

    return true;
}

bool PinholeCamera::sampleDirect(const Vec3f &p, SampleGenerator &sampler, LensSample &sample) const
{
    sample.d = _pos - p;

    if (!evalDirection(sampler, PositionSample(), DirectionSample(-sample.d), sample.weight, sample.pixel))
        return false;

    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.weight /= rSq;
    sample.pdf = 1.0f;
    return true;
}

bool PinholeCamera::evalDirection(SampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    Vec3f localD = _invTransform.transformVector(direction.d);
    if (localD.z() <= 0.0f)
        return false;
    localD *= _planeDist/localD.z();

    pixel.x() = (localD.x() + 1.0f)/_pixelSize.x();
    pixel.y() = (_ratio - localD.y())/_pixelSize.y();
    if (pixel.x() < -_filter.width() || pixel.y() < -_filter.width() ||
        pixel.x() >= _res.x() || pixel.y() >= _res.y())
        return false;

    weight = Vec3f(sqr(_planeDist)/(_pixelSize.x()*_pixelSize.y()*cube(localD.z()/localD.length())));
    return true;
}

float PinholeCamera::directionPdf(const PositionSample &/*point*/, const DirectionSample &direction) const
{
    Vec3f localD = _invTransform.transformVector(direction.d);
    if (localD.z() <= 0.0f)
        return 0.0f;
    localD *= _planeDist/localD.z();

    float u = (localD.x() + 1.0f)*0.5f;
    float v = (1.0f - localD.y()/_ratio)*0.5f;
    if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f)
        return 0.0f;

    return  (_planeDist*2.0f)*(_planeDist*_ratio*2.0f)/cube(localD.z()/localD.length());
}

bool PinholeCamera::isDirac() const
{
    return true;
}

bool PinholeCamera::generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const
{
    float pdf;
    Vec2f uv = _filter.sample(sampler.next2D(), pdf);
    Vec3f dir = _transform.transformVector(Vec3f(
        -1.0f  + (float(pixel.x()) + 0.5f + uv.x())*_pixelSize.x(),
        _ratio - (float(pixel.y()) + 0.5f + uv.y())*_pixelSize.y(),
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
