#include "PinholeCamera.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"

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

void PinholeCamera::fromJson(JsonPtr value, const Scene &scene)
{
    Camera::fromJson(value, scene);
    value.getField("fov", _fovDeg);

    precompute();
}

rapidjson::Value PinholeCamera::toJson(Allocator &allocator) const
{
    return JsonObject{Camera::toJson(allocator), allocator,
        "type", "pinhole",
        "fov", _fovDeg
    };
}

bool PinholeCamera::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.weight = Vec3f(1.0f);
    sample.pdf = 1.0f;
    sample.Ng = _transform.fwd();

    return true;
}

bool PinholeCamera::sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
        Vec2u &pixel, DirectionSample &sample) const
{
    pixel = Vec2u(sampler.next2D()*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool PinholeCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f uv = _filter.sample(sampler.next2D(), pdf);
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

bool PinholeCamera::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    Vec3f localD = _invTransform.transformVector(direction.d);
    if (localD.z() <= 0.0f)
        return false;
    localD *= _planeDist/localD.z();

    Vec2f pixel(
        (localD.x() + 1.0f)/(2.0f*_pixelSize.x()),
        (_ratio - localD.y())/(2.0f*_pixelSize.x())
    );

    Vec2i srcPixel;
    Vec2f xi;
    if (!_filter.invert(Box2i(Vec2i(0), Vec2i(_res)), pixel, sampler.untracked2D(), srcPixel, xi))
        return false;

    sampler.put2D((Vec2f(srcPixel) + sampler.untracked2D())/Vec2f(_res));
    sampler.put2D(xi);

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

bool PinholeCamera::invertPosition(WritablePathSampleGenerator &/*sampler*/, const PositionSample &/*point*/) const
{
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
    if (pixel.x() <= 0.5f - _filter.width() || pixel.y() <= 0.5f - _filter.width() ||
        pixel.x() >= _res.x() - 0.5f + _filter.width() || pixel.y() >= _res.y() - 0.5f + _filter.width())
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
