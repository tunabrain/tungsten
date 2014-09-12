#include "Camera.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <cmath>

namespace Tungsten {

// Default to low-res 16:9
Camera::Camera()
: Camera(Mat4f(), Vec2u(1000u, 563u), 32)
{
}

Camera::Camera(const Mat4f &transform, const Vec2u &res, uint32 spp)
: _outputFile("TungstenRender.png"),
  _tonemapString("gamma"),
  _transform(transform),
  _res(res),
  _spp(spp)
{
    _pos    = _transform*Vec3f(0.0f, 0.0f, 2.0f);
    _lookAt = _transform*Vec3f(0.0f, 0.0f, -1.0f);
    _up     = _transform*Vec3f(0.0f, 1.0f, 0.0f);

    precompute();
}

void Camera::precompute()
{
    _tonemapOp = Tonemap::stringToType(_tonemapString);
    _ratio = _res.y()/float(_res.x());
    _pixelSize = Vec2f(2.0f/_res.x(), 2.0f/_res.x());
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    _invTransform = _transform.pseudoInvert();
}

void Camera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonUtils::fromJson(v, "file", _outputFile);
    JsonUtils::fromJson(v, "tonemap", _tonemapString);
    JsonUtils::fromJson(v, "position", _pos);
    JsonUtils::fromJson(v, "lookAt", _lookAt);
    JsonUtils::fromJson(v, "up", _up);
    JsonUtils::fromJson(v, "resolution", _res);
    JsonUtils::fromJson(v, "spp", _spp);
    if (const rapidjson::Value::Member *medium = v.FindMember("medium"))
        _medium = scene.fetchMedium(medium->value);

    precompute();
}

rapidjson::Value Camera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("file", _outputFile.c_str(), allocator);
    v.AddMember("tonemap", _tonemapString.c_str(), allocator);
    v.AddMember("position", JsonUtils::toJsonValue<float, 3>(_pos,    allocator), allocator);
    v.AddMember("lookAt",   JsonUtils::toJsonValue<float, 3>(_lookAt, allocator), allocator);
    v.AddMember("up",       JsonUtils::toJsonValue<float, 3>(_up,     allocator), allocator);
    v.AddMember("resolution", JsonUtils::toJsonValue<uint32, 2>(_res, allocator), allocator);
    v.AddMember("spp", _spp, allocator);
    if (_medium)
        JsonUtils::addObjectMember(v, "medium", *_medium,  allocator);
    return std::move(v);
}

void Camera::prepareForRender()
{
    _pixels.resize(_res.x()*_res.y(), Vec3d(0.0));
    _weights.resize(_res.x()*_res.y(), 0.0);
}

void Camera::teardownAfterRender()
{
    _pixels.clear();
    _weights.clear();
    _pixels.shrink_to_fit();
    _weights.shrink_to_fit();
}

void Camera::setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up)
{
    _pos = pos;
    _lookAt = lookAt;
    _up = up;
    precompute();
}

void Camera::setPos(const Vec3f &pos)
{
    _pos = pos;
    precompute();
}

void Camera::setLookAt(const Vec3f &lookAt)
{
    _lookAt = lookAt;
    precompute();
}

void Camera::setUp(const Vec3f &up)
{
    _up = up;
    precompute();
}

}
