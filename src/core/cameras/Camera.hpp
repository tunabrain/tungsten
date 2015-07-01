#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "ReconstructionFilter.hpp"
#include "AtomicFramebuffer.hpp"
#include "Tonemap.hpp"

#include "samplerecords/DirectionSample.hpp"
#include "samplerecords/PositionSample.hpp"
#include "samplerecords/LensSample.hpp"

#include "sampling/PathSampleGenerator.hpp"

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

    std::unique_ptr<AtomicFramebuffer> _splatBuffer;
    std::vector<Vec3d> _splats;
    uint64 _splatWeight;

private:
    void precompute();

public:
    Camera();
    Camera(const Mat4f &transform, const Vec2u &res);

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
            DirectionSample &sample) const;
    virtual bool sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const;
    virtual bool evalDirection(PathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const;
    virtual float directionPdf(const PositionSample &point, const DirectionSample &direction) const;

    virtual bool isDirac() const = 0;

    virtual float approximateFov() const = 0;

    virtual void prepareForRender();
    virtual void teardownAfterRender();

    void requestSplatBuffer();
    void blitSplatBuffer(uint64 numSplats);

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

    inline Vec3f tonemap(const Vec3f &c) const
    {
        return Tonemap::tonemap(_tonemapOp, max(c, Vec3f(0.0f)));
    }

    inline Vec3f getLinear(int x, int y) const
    {
        int idx = x + y*_res.x();
        Vec3d result = _pixels[idx]/double(max(_weights[idx], uint32(1)));
        if (!_splats.empty())
            result += _splats[idx]/double(max(_splatWeight, uint64(1)));
        return Vec3f(result);
    }

    inline Vec3f get(int x, int y) const
    {
        return tonemap(getLinear(x, y));
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

    void setResolution(Vec2u res)
    {
        _res = res;
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

    AtomicFramebuffer *splatBuffer()
    {
        return _splatBuffer.get();
    }

    bool isFilterDirac() const
    {
        return _filter.isDirac();
    }

    void setTonemapString(const std::string &name)
    {
        _tonemapString = name;
    }
};

}

#endif /* CAMERA_HPP_ */
