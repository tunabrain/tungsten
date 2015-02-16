#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "ReconstructionFilter.hpp"
#include "Tonemap.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"
#include "io/Path.hpp"

#include <rapidjson/document.h>
#include <vector>
#include <memory>

namespace Tungsten {

class Ray;
class Scene;
class Medium;
class Renderer;
class SampleGenerator;

class Camera : public JsonSerializable
{
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

    std::shared_ptr<Medium> _medium;
    ReconstructionFilter _filter;

    std::vector<Vec3d> _pixels;
    std::vector<uint32> _weights;

private:
    void precompute();

public:
    Camera();
    Camera(const Mat4f &transform, const Vec2u &res);

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const = 0;
    virtual Mat4f approximateProjectionMatrix(int width, int height) const = 0;
    virtual float approximateFov() const = 0;

    virtual void prepareForRender();
    virtual void teardownAfterRender();

    void setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up);
    void setPos(const Vec3f &pos);
    void setLookAt(const Vec3f &lookAt);
    void setUp(const Vec3f &up);

    void addSamples(int x, int y, const Vec3f &c, uint32 weight)
    {
        int idx = x + y*_res.x();
        _pixels[idx] += Vec3d(c);
        _weights[idx] += weight;
    }

    Vec3f tonemap(const Vec3f &c) const
    {
        return Tonemap::tonemap(_tonemapOp, max(c, Vec3f(0.0f)));
    }

    Vec3f getLinear(int x, int y) const
    {
        int idx = x + y*_res.x();
        return Vec3f(_pixels[idx]/_weights[idx]);
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

    const std::shared_ptr<Medium> &medium() const
    {
        return _medium;
    }

    Tonemap::Type tonemapOp() const
    {
        return _tonemapOp;
    }

    std::vector<Vec3d> &pixels()
    {
        return _pixels;
    }

    std::vector<uint32> &weights()
    {
        return _weights;
    }
};

}

#endif /* CAMERA_HPP_ */
