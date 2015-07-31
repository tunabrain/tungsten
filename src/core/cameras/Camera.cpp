#include "Camera.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/FileUtils.hpp"
#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <iostream>
#include <cmath>

namespace Tungsten {

template<typename T>
std::unique_ptr<T[]> zeroAlloc(size_t size)
{
    std::unique_ptr<T[]> result(new T[size]);
    std::memset(result.get(), 0, size*sizeof(T));
    return std::move(result);
}

// Default to low-res 16:9
Camera::Camera()
: Camera(Mat4f(), Vec2u(1000u, 563u))
{
}

Camera::Camera(const Mat4f &transform, const Vec2u &res)
: _tonemapString("gamma"),
  _transform(transform),
  _res(res)
{
    _pos    = _transform*Vec3f(0.0f, 0.0f, 2.0f);
    _lookAt = _transform*Vec3f(0.0f, 0.0f, -1.0f);
    _up     = _transform*Vec3f(0.0f, 1.0f, 0.0f);

    _transform.setRight(-_transform.right());

    precompute();
}

void Camera::precompute()
{
    _tonemapOp = Tonemap::stringToType(_tonemapString);
    _ratio = _res.y()/float(_res.x());
    _pixelSize = Vec2f(1.0f/_res.x(), 1.0f/_res.y());
    _invTransform = _transform.pseudoInvert();
}

void Camera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonUtils::fromJson(v, "tonemap", _tonemapString);
    JsonUtils::fromJson(v, "resolution", _res);
    if (const rapidjson::Value::Member *medium = v.FindMember("medium"))
        _medium = scene.fetchMedium(medium->value);
    if (const rapidjson::Value::Member *filter = v.FindMember("reconstruction_filter"))
        if (filter->value.IsString())
            _filter = ReconstructionFilter(filter->value.GetString());

    if (const rapidjson::Value::Member *transform = v.FindMember("transform")) {
        JsonUtils::fromJson(transform->value, _transform);
        _pos    = _transform.extractTranslationVec();
        _lookAt = _transform.fwd() + _pos;
        _up     = _transform.up();

        if (transform->value.IsObject()) {
            JsonUtils::fromJson(transform->value, "up", _up);
            JsonUtils::fromJson(transform->value, "look_at", _lookAt);
        }

        _transform.setRight(-_transform.right());
    }

    precompute();
}

rapidjson::Value Camera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("tonemap", _tonemapString.c_str(), allocator);
    v.AddMember("resolution", JsonUtils::toJsonValue<uint32, 2>(_res, allocator), allocator);
    if (_medium)
        JsonUtils::addObjectMember(v, "medium", *_medium,  allocator);
    v.AddMember("reconstruction_filter", _filter.name().c_str(), allocator);

    rapidjson::Value tform(rapidjson::kObjectType);
    tform.AddMember("position", JsonUtils::toJsonValue<float, 3>(_pos,    allocator), allocator);
    tform.AddMember("look_at",  JsonUtils::toJsonValue<float, 3>(_lookAt, allocator), allocator);
    tform.AddMember("up",       JsonUtils::toJsonValue<float, 3>(_up,     allocator), allocator);
    v.AddMember("transform", std::move(tform), allocator);

    return std::move(v);
}

bool Camera::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &/*sample*/) const
{
    return false;
}

bool Camera::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        DirectionSample &/*sample*/) const
{
    return false;
}

bool Camera::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/, Vec2u /*pixel*/,
        DirectionSample &/*sample*/) const
{
    return false;
}

bool Camera::sampleDirect(const Vec3f &/*p*/, PathSampleGenerator &/*sampler*/, LensSample &/*sample*/) const
{
    return false;
}

bool Camera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &/*direction*/, Vec3f &/*weight*/, Vec2f &/*pixel*/) const
{
    return false;
}

float Camera::directionPdf(const PositionSample &/*point*/, const DirectionSample &/*direction*/) const
{
    return 0.0f;
}

void Camera::prepareForRender()
{
    precompute();
}

void Camera::teardownAfterRender()
{
    _colorBuffer.reset();
    _sampleCount.reset();
    _splatBuffer.reset();
}

void Camera::requestColorBuffer()
{
    _colorBuffer = zeroAlloc<Vec3f> (_res.product());
    _sampleCount = zeroAlloc<uint32>(_res.product());
    _colorBufferWeight = 1.0;
}

void Camera::requestSplatBuffer()
{
    _splatBuffer.reset(new AtomicFramebuffer(_res.x(), _res.y(), _filter));
    _splatWeight = 1.0;
}

void Camera::blitSplatBuffer()
{
    for (uint32 y = 0; y < _res.y(); ++y) {
        for (uint32 x = 0; x < _res.x(); ++x) {
            _colorBuffer[x + y*_res.x()] = _splatBuffer->get(x, y);
            _sampleCount[x + y*_res.x()] = 1;
        }
    }
    _splatBuffer->unsafeReset();
}

void Camera::setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up)
{
    _pos = pos;
    _lookAt = lookAt;
    _up = up;
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    precompute();
}

void Camera::setPos(const Vec3f &pos)
{
    _pos = pos;
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    precompute();
}

void Camera::setLookAt(const Vec3f &lookAt)
{
    _lookAt = lookAt;
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    precompute();
}

void Camera::setUp(const Vec3f &up)
{
    _up = up;
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    precompute();
}

}
