#ifndef PINHOLECAMERA_HPP_
#define PINHOLECAMERA_HPP_

#include "Camera.hpp"

namespace Tungsten {

class Scene;

class PinholeCamera : public Camera
{
    float _fovDeg;
    float _fovRad;
    float _planeDist;
    float _invPlaneArea;

    void precompute();

public:
    PinholeCamera();
    PinholeCamera(const Mat4f &transform, const Vec2u &res, float fov);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override final;
    virtual bool sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
            Vec2u &pixel, DirectionSample &sample) const override final;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
            DirectionSample &sample) const override final;
    virtual bool sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const override final;
    virtual bool invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const override final;
    virtual bool invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &sample) const override final;
    virtual bool evalDirection(PathSampleGenerator &sampler, const PositionSample &point,
                const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const override final;
    virtual float directionPdf(const PositionSample &point, const DirectionSample &direction) const override final;

    virtual bool isDirac() const override;

    virtual float approximateFov() const override
    {
        return _fovRad;
    }

    float fovDeg() const
    {
        return _fovDeg;
    }
};

}

#endif /* PINHOLECAMERA_HPP_ */
