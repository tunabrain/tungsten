#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "Tonemap.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

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
    std::string _outputFile;
    std::string _hdrOutputFile;
    std::string _varianceOutputFile;
    std::string _tonemapString;
    bool _overwriteOutputFiles;

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
    void precompute();

    std::string incrementalFilename(const std::string &dstFile, const std::string &suffix) const;
    void saveBuffers(Renderer &renderer, const std::string &suffix) const;

public:
    Camera();
    Camera(const Mat4f &transform, const Vec2u &res, uint32 spp);

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

    void saveOutputs(Renderer &renderer) const;
    void saveCheckpoint(Renderer &renderer) const;

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

    uint32 spp() const
    {
        return _spp;
    }

    const std::string &outputFile() const
    {
        return _outputFile;
    }

    const std::string &hdrOutputFile() const
    {
        return _hdrOutputFile;
    }

    const std::string &varianceOutputFile() const
    {
        return _varianceOutputFile;
    }

    bool overwriteOutputFiles() const
    {
        return _overwriteOutputFiles;
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
