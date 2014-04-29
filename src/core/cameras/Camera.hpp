#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include <rapidjson/document.h>
#include <cmath>

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"


namespace Tungsten
{

class Scene;

class Camera : public JsonSerializable
{
    std::string _outputFile;

protected:
    Mat4f _transform;
    Mat4f _invTransform;
    Vec3f _pos;
    Vec3f _lookAt;
    Vec3f _up;

    Vec2u _res;
    float _ratio;
    Vec2f _pixelSize;

    uint32 _spp;

private:
    void precompute()
    {
        _ratio = _res.y()/float(_res.x());
        _pixelSize = Vec2f(2.0f/_res.x(), 2.0f/_res.x());
        _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
        _invTransform = _transform.pseudoInvert();
    }

public:
    Camera()
    : Camera(Mat4f(), Vec2u(800u, 600u), 256)
    {
    }

    Camera(const Mat4f &transform, const Vec2u &res, uint32 spp)
    : _outputFile("Frame.png"),
      _transform(transform),
      _res(res),
      _spp(spp)
    {
        _pos    = _transform*Vec3f(0.0f, 0.0f, 2.0f);
        _lookAt = _transform*Vec3f(0.0f, 0.0f, -1.0f);
        _up     = _transform*Vec3f(0.0f, 1.0f, 0.0f);

        precompute();
    }

    void fromJson(const rapidjson::Value &v, const Scene &/*scene*/) override
    {
        JsonUtils::fromJson(v, "file", _outputFile);
        JsonUtils::fromJson(v, "position", _pos);
        JsonUtils::fromJson(v, "lookAt", _lookAt);
        JsonUtils::fromJson(v, "up", _up);
        JsonUtils::fromJson(v, "resolution", _res);
        JsonUtils::fromJson(v, "spp", _spp);

        precompute();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = JsonSerializable::toJson(allocator);
        v.AddMember("file", _outputFile.c_str(), allocator);
        v.AddMember("position", JsonUtils::toJsonValue<float, 3>(_pos,    allocator), allocator);
        v.AddMember("lookAt",   JsonUtils::toJsonValue<float, 3>(_lookAt, allocator), allocator);
        v.AddMember("up",       JsonUtils::toJsonValue<float, 3>(_up,     allocator), allocator);
        v.AddMember("resolution", JsonUtils::toJsonValue<uint32, 2>(_res, allocator), allocator);
        v.AddMember("spp", _spp, allocator);
        return std::move(v);
    }

    virtual Ray generateSample(Vec2u pixel, SampleGenerator &sampler) const = 0;
    virtual Mat4f approximateProjectionMatrix(int width, int height) const = 0;
    virtual float approximateFov() const = 0;

    const Mat4f &transform() const
    {
        return _transform;
    }

    const Vec3f &pos() const
    {
        return _pos;
    }

    const Vec3f &lookAt() const
    {
        return _lookAt;
    }

    const Vec3f &up() const
    {
        return _up;
    }

    const Vec2u &resolution() const
    {
        return _res;
    }

    uint32 spp() const
    {
        return _spp;
    }

    const std::string &outputFile() const
    {
        return _outputFile;
    }

    void setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up)
    {
        _pos = pos;
        _lookAt = lookAt;
        _up = up;
        precompute();
    }

    void setPos(const Vec3f &pos)
    {
        _pos = pos;
        precompute();
    }

    void setLookAt(const Vec3f &lookAt)
    {
        _lookAt = lookAt;
        precompute();
    }

    void setUp(const Vec3f &up)
    {
        _up = up;
        precompute();
    }
};

}

#endif /* CAMERA_HPP_ */
