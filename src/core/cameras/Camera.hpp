#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "ReconstructionFilter.hpp"
#include "AtomicFramebuffer.hpp"
#include "OutputBuffer.hpp"
#include "Tonemap.hpp"

#include "samplerecords/DirectionSample.hpp"
#include "samplerecords/PositionSample.hpp"
#include "samplerecords/LensSample.hpp"

#include "sampling/WritablePathSampleGenerator.hpp"

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

    OutputBufferSettings _colorBufferSettings;

    std::unique_ptr<OutputBufferVec3f> _colorBuffer;
    std::unique_ptr<OutputBufferF>     _depthBuffer;
    std::unique_ptr<OutputBufferVec3f> _normalBuffer;
    std::unique_ptr<OutputBufferVec3f> _albedoBuffer;
    std::unique_ptr<OutputBufferF> _visibilityBuffer;

    double _colorBufferWeight;

    std::unique_ptr<AtomicFramebuffer> _splatBuffer;
    double _splatWeight;

private:
    void precompute();

public:
    Camera();
    Camera(const Mat4f &transform, const Vec2u &res);

    void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const;
    virtual bool sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
            Vec2u &pixel, DirectionSample &sample) const;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
            DirectionSample &sample) const;
    virtual bool sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const;
    virtual bool invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const;
    virtual bool invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &sample) const;
    virtual bool evalDirection(PathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const;
    virtual float directionPdf(const PositionSample &point, const DirectionSample &direction) const;

    virtual bool isDirac() const = 0;

    virtual float approximateFov() const = 0;

    virtual void prepareForRender();
    virtual void teardownAfterRender();

    void requestOutputBuffers(const std::vector<OutputBufferSettings> &settings);
    void requestColorBuffer();
    void requestSplatBuffer();
    void blitSplatBuffer();

    void setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up);
    void setPos(const Vec3f &pos);
    void setLookAt(const Vec3f &lookAt);
    void setUp(const Vec3f &up);

    void saveOutputBuffers() const;
    void serializeOutputBuffers(OutputStreamHandle &out) const;
    void deserializeOutputBuffers(InputStreamHandle &in);

    OutputBufferVec3f *colorBuffer()
    {
        return _colorBuffer.get();
    }

    OutputBufferF *depthBuffer()
    {
        return _depthBuffer.get();
    }

    OutputBufferVec3f *normalBuffer()
    {
        return _normalBuffer.get();
    }

    OutputBufferVec3f *albedoBuffer()
    {
        return _albedoBuffer.get();
    }

    OutputBufferF *visibilityBuffer()
    {
        return _visibilityBuffer.get();
    }

    const OutputBufferVec3f *colorBuffer() const
    {
        return _colorBuffer.get();
    }

    const OutputBufferF *depthBuffer() const
    {
        return _depthBuffer.get();
    }

    const OutputBufferVec3f *normalBuffer() const
    {
        return _normalBuffer.get();
    }

    const OutputBufferVec3f *albedoBuffer() const
    {
        return _albedoBuffer.get();
    }

    const OutputBufferF *visibilityBuffer() const
    {
        return _visibilityBuffer.get();
    }

    inline Vec3f tonemap(const Vec3f &c) const
    {
        return Tonemap::tonemap(_tonemapOp, max(c, Vec3f(0.0f)));
    }

    inline Vec3f getLinear(int x, int y) const
    {
        int idx = x + y*_res.x();
        Vec3f result(0.0f);
        if (_colorBuffer)
            result += (*_colorBuffer)[idx]*_colorBufferWeight;
        if (_splatBuffer)
            result += Vec3f(Vec3d(_splatBuffer->get(x, y))*_splatWeight);
        return result;
    }

    void setColorBufferWeight(double weight)
    {
        _colorBufferWeight = weight;
    }

    void setSplatWeight(double weight)
    {
        _splatWeight = weight;
    }

    inline Vec3f get(int x, int y) const
    {
        return tonemap(getLinear(x, y));
    }

    const Mat4f &transform() const
    {
        return _transform;
    }

    const Mat4f &invTransform() const
    {
        return _invTransform;
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

    AtomicFramebuffer *splatBuffer()
    {
        return _splatBuffer.get();
    }

    ReconstructionFilter reconstructionFilter() const
    {
        return _filter;
    }

    bool isFilterDirac() const
    {
        return _filter.isDirac();
    }

    void setTonemapString(const std::string &name)
    {
        _tonemapOp = name;
    }
};

}

#endif /* CAMERA_HPP_ */
