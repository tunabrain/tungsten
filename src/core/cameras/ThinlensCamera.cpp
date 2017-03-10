#include "ThinlensCamera.hpp"

#include "textures/DiskTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Angle.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include <cmath>

namespace Tungsten {

ThinlensCamera::ThinlensCamera()
: Camera(),
  _scene(nullptr),
  _fovDeg(60.0f),
  _focusDist(1.0f),
  _apertureSize(0.001f),
  _catEye(0.0f),
  _aperture(std::make_shared<DiskTexture>())
{
    precompute();
}

void ThinlensCamera::precompute()
{
    _fovRad = Angle::degToRad(_fovDeg);
    _planeDist = 1.0f/std::tan(_fovRad*0.5f);
    _aperture->makeSamplable(MAP_UNIFORM);

    float planeArea = (2.0f/_planeDist)*(2.0f*_ratio/_planeDist);
    _invPlaneArea = 1.0f/planeArea;
}

float ThinlensCamera::evalApertureThroughput(Vec3f planePos, Vec2f aperturePos) const
{
    float aperture = (*_aperture)[aperturePos].x();

    if (_catEye > 0.0f) {
        aperturePos = (aperturePos*2.0f - 1.0f)*_apertureSize;
        Vec3f lensPos = Vec3f(aperturePos.x(), aperturePos.y(), 0.0f);
        Vec3f localDir = (planePos - lensPos).normalized();
        Vec2f diaphragmPos = lensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return 0.0f;
    }
    return aperture/_aperture->maximum().x();
}

void ThinlensCamera::fromJson(JsonPtr value, const Scene &scene)
{
    _scene = &scene;
    Camera::fromJson(value, scene);
    value.getField("fov", _fovDeg);
    value.getField("focus_distance", _focusDist);
    value.getField("aperture_size", _apertureSize);
    value.getField("cateye", _catEye);
    value.getField("focus_pivot", _focusPivot);
    if (auto aperture = value["aperture"])
        _aperture = scene.fetchTexture(aperture, TexelConversion::REQUEST_AVERAGE);

    precompute();
}

rapidjson::Value ThinlensCamera::toJson(Allocator &allocator) const
{
    JsonObject result{Camera::toJson(allocator), allocator,
        "type", "thinlens",
        "fov", _fovDeg,
        "focus_distance", _focusDist,
        "aperture_size", _apertureSize,
        "cateye", _catEye,
        "aperture", *_aperture
    };
    if (!_focusPivot.empty())
        result.add("focus_pivot", _focusPivot);

    return result;
}

bool ThinlensCamera::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    Vec2f lensUv = sampler.next2D();
    Vec2f aperturePos = _aperture->sample(MAP_UNIFORM, lensUv);
    aperturePos = (aperturePos*2.0f - 1.0f)*_apertureSize;

    sample.p = _transform*Vec3f(aperturePos.x(), aperturePos.y(), 0.0f);
    sample.weight = Vec3f(1.0f);
    sample.pdf = _aperture->pdf(MAP_UNIFORM, lensUv)*sqr(0.5f/_apertureSize);
    sample.Ng = _transform.fwd();

    return true;
}

bool ThinlensCamera::sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
        Vec2u &pixel, DirectionSample &sample) const
{
    pixel = Vec2u(sampler.next2D()*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool ThinlensCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f pixelUv = _filter.sample(sampler.next2D(), pdf);
    Vec3f planePos = Vec3f(
        -1.0f  + (float(pixel.x()) + pixelUv.x())*2.0f*_pixelSize.x(),
        _ratio - (float(pixel.y()) + pixelUv.y())*2.0f*_pixelSize.x(),
        _planeDist
    );
    planePos *= _focusDist/planePos.z();

    Vec3f lensPos = _invTransform*point.p;
    Vec3f localD = (planePos - lensPos).normalized();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = lensPos.xy() - _catEye*_planeDist*localD.xy()/localD.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    sample.d =  _transform.transformVector(localD);
    sample.weight = Vec3f(1.0f);
    sample.pdf = _invPlaneArea/cube(localD.z());

    return true;
}

bool ThinlensCamera::sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const
{
    PositionSample point;
    if (!samplePosition(sampler, point))
        return false;

    sample.d = point.p - p;
    if (!evalDirection(sampler, point, DirectionSample(-sample.d), sample.weight, sample.pixel))
        return false;

    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.weight *= point.weight/rSq;
    return true;
}

bool ThinlensCamera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &point,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    Vec3f localLensPos = _invTransform*point.p;
    Vec3f localDir = _invTransform.transformVector(direction.d);
    if (localDir.z() <= 0.0f)
        return false;

    Vec3f planePos = localDir*(_focusDist/localDir.z()) + localLensPos;
    planePos *= _planeDist/planePos.z();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = localLensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    pixel.x() = (planePos.x() + 1.0f)/(2.0f*_pixelSize.x());
    pixel.y() = (_ratio - planePos.y())/(2.0f*_pixelSize.x());
    if (pixel.x() < -_filter.width() || pixel.y() < -_filter.width() ||
        pixel.x() >= _res.x() || pixel.y() >= _res.y())
        return false;

    weight = Vec3f(sqr(_planeDist)/(4.0f*_pixelSize.x()*_pixelSize.x()*cube(localDir.z()/localDir.length())));
    return true;
}

float ThinlensCamera::directionPdf(const PositionSample &point, const DirectionSample &direction) const
{
    Vec3f localLensPos = _invTransform*point.p;
    Vec3f localDir = _invTransform.transformVector(direction.d);
    if (localDir.z() <= 0.0f)
        return false;

    Vec3f planePos = localDir*(_focusDist/localDir.z()) + localLensPos;
    planePos *= _planeDist/planePos.z();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = localLensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    float u = (planePos.x() + 1.0f)*0.5f;
    float v = (1.0f - planePos.y()/_ratio)*0.5f;
    if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f)
        return 0.0f;

    return  _invPlaneArea/cube(localDir.z()/localDir.length());
}

bool ThinlensCamera::isDirac() const
{
    return false;
}

void ThinlensCamera::prepareForRender()
{
    Camera::prepareForRender();

    if (!_focusPivot.empty()) {
        const Primitive *prim = _scene->findPrimitive(_focusPivot);

        if (prim)
            _focusDist = (prim->transform()*Vec3f(0.0f) - _pos).length();
        else
            DBG("Warning: Focus pivot '%s' for thinlens camera not found", _focusPivot);
    }
}

}
