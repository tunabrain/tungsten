#ifndef CUBEMAPCAMERA_HPP_
#define CUBEMAPCAMERA_HPP_

#include "Camera.hpp"

#include "StringableEnum.hpp"

#include <array>

namespace Tungsten {

class Scene;

class CubemapCamera : public Camera
{
    enum ProjectionModeEnum
    {
        MODE_HORIZONTAL_CROSS,
        MODE_VERTICAL_CROSS,
        MODE_ROW,
        MODE_COLUMN,
    };
    typedef StringableEnum<ProjectionModeEnum> ProjectionMode;
    friend ProjectionMode;

    ProjectionMode _mode;
    Mat4f _rot;
    Mat4f _invRot;
    std::array<Vec3f, 6> _basisU;
    std::array<Vec3f, 6> _basisV;
    std::array<Vec2f, 6> _faceOffset;
    float _visibleArea;
    Vec2f _faceSize;

    inline void directionToFace(const Vec3f &d, int &face, Vec2f &offset) const;
    inline Vec3f faceToDirection(int face, Vec2f offset) const;
    inline bool uvToFace(Vec2f uv, int &face) const;

    inline Vec2f directionToUV(const Vec3f &wi, float &pdf) const;
    inline Vec3f uvToDirection(int face, Vec2f uv, float &pdf) const;

public:
    CubemapCamera();
    CubemapCamera(const Mat4f &transform, const Vec2u &res);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
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

#endif /* CUBEMAPCAMERA_HPP_ */
