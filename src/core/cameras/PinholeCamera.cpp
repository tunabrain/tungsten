#include "PinholeCamera.hpp"

#include "sampling/PathSampleGenerator.hpp"

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

    float planeArea = (2.0f/_planeDist)*(2.0f*_ratio/_planeDist);
    _invPlaneArea = 1.0f/planeArea;
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

bool PinholeCamera::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.weight = Vec3f(1.0f);
    sample.pdf = 1.0f;
    sample.Ng = _transform.fwd();

    return true;
}

bool PinholeCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point,
        DirectionSample &sample) const
{
    Vec2u pixel(sampler.next2D(CameraSample)*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool PinholeCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f uv = _filter.sample(sampler.next2D(CameraSample), pdf);
    Vec3f localD = Vec3f(
        -1.0f  + (float(pixel.x()) + 0.5f + uv.x())*2.0f*_pixelSize.x(),
        _ratio - (float(pixel.y()) + 0.5f + uv.y())*2.0f*_pixelSize.x(),
        _planeDist
    ).normalized();

    sample.d =  _transform.transformVector(localD);
    sample.weight = Vec3f(1.0f);
    sample.pdf = _invPlaneArea/cube(localD.z());

    return true;
}

bool PinholeCamera::sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const
{
    sample.d = _pos - p;

    if (!evalDirection(sampler, PositionSample(), DirectionSample(-sample.d), sample.weight, sample.pixel))
        return false;

    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.weight /= rSq;
    return true;
}

bool PinholeCamera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    Vec3f localD = _invTransform.transformVector(direction.d);
    if (localD.z() <= 0.0f)
        return false;
    localD *= _planeDist/localD.z();

    pixel.x() = (localD.x() + 1.0f)/(2.0f*_pixelSize.x());
    pixel.y() = (_ratio - localD.y())/(2.0f*_pixelSize.x());
    if (pixel.x() < -_filter.width() || pixel.y() < -_filter.width() ||
        pixel.x() >= _res.x() || pixel.y() >= _res.y())
        return false;

    weight = Vec3f(sqr(_planeDist)/(4.0f*_pixelSize.x()*_pixelSize.x()*cube(localD.z()/localD.length())));
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

    return  _invPlaneArea/cube(localD.z()/localD.length());
}

bool PinholeCamera::isDirac() const
{
    return true;
}

}
