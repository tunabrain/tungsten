#include "EquirectangularCamera.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"

#include <cmath>

namespace Tungsten {

EquirectangularCamera::EquirectangularCamera()
: Camera()
{
}

EquirectangularCamera::EquirectangularCamera(const Mat4f &transform, const Vec2u &res)
: Camera(transform, res)
{
}

Vec2f EquirectangularCamera::directionToUV(const Vec3f &wi, float &sinTheta) const
{
    Vec3f wLocal = _invRot*wi;
    sinTheta = std::sqrt(max(1.0f - wLocal.y()*wLocal.y(), 0.0f));
    return Vec2f(std::atan2(wLocal.z(), wLocal.x())*INV_TWO_PI + 0.5f, 1.0f - std::acos(clamp(-wLocal.y(), -1.0f, 1.0f))*INV_PI);
}

Vec3f EquirectangularCamera::uvToDirection(Vec2f uv, float &sinTheta) const
{
    float phi   = (uv.x() - 0.5f)*TWO_PI;
    float theta = (1.0f - uv.y())*PI;
    sinTheta = std::sin(theta);
    return _rot*Vec3f(
        std::cos(phi)*sinTheta,
        -std::cos(theta),
        std::sin(phi)*sinTheta
    );
}

rapidjson::Value EquirectangularCamera::toJson(Allocator &allocator) const
{
    return JsonObject{Camera::toJson(allocator), allocator,
        "type", "equirectangular"
    };
}

bool EquirectangularCamera::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.weight = Vec3f(1.0f);
    sample.pdf = 1.0f;
    sample.Ng = _transform.fwd();

    return true;
}

bool EquirectangularCamera::sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
        Vec2u &pixel, DirectionSample &sample) const
{
    pixel = Vec2u(sampler.next2D()*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool EquirectangularCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f uv = (Vec2f(pixel) + 0.5f + _filter.sample(sampler.next2D(), pdf))*_pixelSize;

    float sinTheta;
    sample.d = uvToDirection(uv, sinTheta);
    sample.weight = Vec3f(1.0f);
    sample.pdf = INV_PI*INV_TWO_PI/sinTheta;

    return true;
}

bool EquirectangularCamera::sampleDirect(const Vec3f &p, PathSampleGenerator &/*sampler*/, LensSample &sample) const
{
    sample.d = _pos - p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;

    float sinTheta;
    Vec2f uv = directionToUV(-sample.d, sinTheta);

    sample.pixel = uv/_pixelSize;
    sample.weight = Vec3f(INV_PI*INV_TWO_PI/(sinTheta*_pixelSize.x()*_pixelSize.y()*rSq));

    return true;
}

bool EquirectangularCamera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    float sinTheta;
    Vec2f uv = directionToUV(direction.d, sinTheta);

    pixel = uv/_pixelSize;
    weight = Vec3f(INV_PI*INV_TWO_PI/(sinTheta*_pixelSize.x()*_pixelSize.y()));

    return true;
}

float EquirectangularCamera::directionPdf(const PositionSample &/*point*/, const DirectionSample &direction) const
{
    float sinTheta;
    directionToUV(direction.d, sinTheta);

    return INV_PI*INV_TWO_PI/sinTheta;
}

bool EquirectangularCamera::isDirac() const
{
    return true;
}

float EquirectangularCamera::approximateFov() const
{
    return 90.0f;
}

void EquirectangularCamera::prepareForRender()
{
    _rot = _transform.extractRotation();
    _invRot = _rot.transpose();

    Camera::prepareForRender();
}

}
