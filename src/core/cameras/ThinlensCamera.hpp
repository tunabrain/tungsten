#ifndef THINLENSCAMERA_HPP_
#define THINLENSCAMERA_HPP_

#include "Camera.hpp"

#include "materials/Texture.hpp"

namespace Tungsten {

class Scene;

class ThinlensCamera : public Camera
{
    const Scene *_scene;

    float _fovDeg;
    float _fovRad;
    float _planeDist;
    float _focusDist;
    float _apertureSize;
    float _chromaticAberration;
    float _catEye;
    std::string _focusPivot;

    std::shared_ptr<Texture> _aperture;

    void precompute();

    float evalApertureThroughput(Vec3f planePos, Vec2f aperturePos) const;
    Vec3f aberration(const Vec3f &planePos, Vec2u pixel, Vec2f &aperturePos, SampleGenerator &sampler) const;

public:
    ThinlensCamera();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool generateSample(Vec2u pixel, SampleGenerator &sampler, Vec3f &throughput, Ray &ray) const override;

    virtual void prepareForRender() override;

    virtual Mat4f approximateProjectionMatrix(int width, int height) const override;

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
