#include "ThinlensCamera.hpp"

#include "materials/DiskTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Angle.hpp"

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

void ThinlensCamera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    _scene = &scene;
    Camera::fromJson(v, scene);
    JsonUtils::fromJson(v, "fov", _fovDeg);
    JsonUtils::fromJson(v, "focus_distance", _focusDist);
    JsonUtils::fromJson(v, "aperture_size", _apertureSize);
    JsonUtils::fromJson(v, "cateye", _catEye);
    JsonUtils::fromJson(v, "focus_pivot", _focusPivot);
    scene.textureFromJsonMember(v, "aperture", TexelConversion::REQUEST_AVERAGE, _aperture);

    precompute();
}

rapidjson::Value ThinlensCamera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Camera::toJson(allocator);
    v.AddMember("type", "thinlens", allocator);
    v.AddMember("fov", _fovDeg, allocator);
    v.AddMember("focus_distance", _focusDist, allocator);
    v.AddMember("aperture_size", _apertureSize, allocator);
    v.AddMember("cateye", _catEye, allocator);
    if (!_focusPivot.empty())
        v.AddMember("focus_pivot", _focusPivot.c_str(), allocator);
    JsonUtils::addObjectMember(v, "aperture", *_aperture, allocator);
    return std::move(v);
}

bool ThinlensCamera::generateSample(Vec2u pixel, PathSampleGenerator &sampler, Vec3f &throughput, Ray &ray) const
{
    float filterPdf;
    Vec2f pixelUv = 0.5f + _filter.sample(sampler.next2D(CameraSample), filterPdf);
    Vec2f lensUv = sampler.next2D(CameraSample);

    Vec3f planePos = Vec3f(
        -1.0f  + (float(pixel.x()) + pixelUv.x())*2.0f*_pixelSize.x(),
        _ratio - (float(pixel.y()) + pixelUv.y())*2.0f*_pixelSize.x(),
        _planeDist
    );
    planePos *= _focusDist/planePos.z();

    Vec2f aperturePos = _aperture->sample(MAP_UNIFORM, lensUv);
    throughput = Vec3f(1.0f);
    aperturePos = (aperturePos*2.0f - 1.0f)*_apertureSize;

    Vec3f lensPos = Vec3f(aperturePos.x(), aperturePos.y(), 0.0f);
    Vec3f localDir = (planePos - lensPos).normalized();
    Vec3f dir = _transform.transformVector(localDir);

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = lensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    ray = Ray(_transform*lensPos, dir);

    return true;
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
