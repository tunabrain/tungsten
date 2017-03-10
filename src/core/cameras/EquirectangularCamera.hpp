#ifndef EQUIRECTANGULARCAMERA_HPP_
#define EQUIRECTANGULARCAMERA_HPP_

#include "Camera.hpp"

namespace Tungsten {

class Scene;

class EquirectangularCamera : public Camera
{
    Mat4f _rot;
    Mat4f _invRot;

    Vec2f directionToUV(const Vec3f &wi, float &sinTheta) const;
    Vec3f uvToDirection(Vec2f uv, float &sinTheta) const;

public:
    EquirectangularCamera();
    EquirectangularCamera(const Mat4f &transform, const Vec2u &res);

    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override final;
    virtual bool sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
            Vec2u &pixel, DirectionSample &sample) const override final;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
            DirectionSample &sample) const override final;
    virtual bool sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const override final;
    virtual bool evalDirection(PathSampleGenerator &sampler, const PositionSample &point,
                const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const override final;
    virtual float directionPdf(const PositionSample &point, const DirectionSample &direction) const override final;

    virtual bool isDirac() const override;

    virtual float approximateFov() const override;

    virtual void prepareForRender() override;
};

}

#endif /* EQUIRECTANGULARCAMERA_HPP_ */
