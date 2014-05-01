#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "Tonemap.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"
#include "math/Ray.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>
#include <vector>
#include <memory>
#include <cmath>

namespace Tungsten
{

class Scene;
class Medium;
class SampleGenerator;

class Camera : public JsonSerializable
{
    std::string _outputFile;
    std::string _tonemapString;

    Tonemap::Type _tonemapOp;

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

    std::shared_ptr<Medium> _medium;

    std::vector<Vec3d> _pixels;
    std::vector<uint32> _weights;

private:
    void precompute()
    {
        _tonemapOp = Tonemap::stringToType(_tonemapString);
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

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Ray generateSample(Vec2u pixel, SampleGenerator &sampler) const = 0;
    virtual Mat4f approximateProjectionMatrix(int width, int height) const = 0;
    virtual float approximateFov() const = 0;

    void prepareForRender()
    {
        _pixels.resize(_res.x()*_res.y(), Vec3d(0.0));
        _weights.resize(_res.x()*_res.y(), 0.0);
    }

    void teardownAfterRender()
    {
        _pixels.clear();
        _weights.clear();
        _pixels.shrink_to_fit();
        _weights.shrink_to_fit();
    }

    void addSamples(int x, int y, const Vec3f &c, uint32 weight)
    {
        int idx = x + y*_res.x();
        _pixels[idx] += Vec3d(c);
        _weights[idx] += weight;
    }

    Vec3f tonemap(const Vec3f &c) const
    {
        return Tonemap::tonemap(_tonemapOp, c);
    }

    Vec3f get(int x, int y) const
    {
        int idx = x + y*_res.x();
        return tonemap(Vec3f(_pixels[idx]/_weights[idx]));
    }

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

    const std::shared_ptr<Medium> &medium() const
    {
        return _medium;
    }

    Tonemap::Type tonemapOp() const
    {
        return _tonemapOp;
    }
};

}

#endif /* CAMERA_HPP_ */
