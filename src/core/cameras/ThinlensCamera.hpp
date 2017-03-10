#ifndef THINLENSCAMERA_HPP_
#define THINLENSCAMERA_HPP_

#include "Camera.hpp"

#include "textures/Texture.hpp"

namespace Tungsten {

class Scene;

class ThinlensCamera : public Camera
{
    const Scene *_scene;

    float _fovDeg;
    float _fovRad;
    float _planeDist;
    float _invPlaneArea;
    float _focusDist;
    float _apertureSize;
    float _catEye;
    std::string _focusPivot;

    std::shared_ptr<Texture> _aperture;

    void precompute();

    float evalApertureThroughput(Vec3f planePos, Vec2f aperturePos) const;

public:
    ThinlensCamera();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override final;
    virtual bool sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point, Vec2u &pixel, DirectionSample &sample) const override;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
            DirectionSample &sample) const override;
    virtual bool sampleDirect(const Vec3f &p, PathSampleGenerator &sampler, LensSample &sample) const override;
    virtual bool evalDirection(PathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const override;
    virtual float directionPdf(const PositionSample &point, const DirectionSample &direction) const override;

    virtual bool isDirac() const override;

    virtual void prepareForRender() override;

    virtual float approximateFov() const override
    {
        return _fovRad;
    }

    float fovDeg() const
    {
        return _fovDeg;
    }

    float apertureSize() const
    {
        return _apertureSize;
    }

    float focusDist() const
    {
        return _focusDist;
    }
};

}

#endif /* THINLENSCAMERA_HPP_ */
