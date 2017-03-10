#include "CubemapCamera.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"

#include <cmath>

namespace Tungsten {

static const int PosX = 0, NegX = 1;
static const int PosY = 2, NegY = 3;
static const int PosZ = 4, NegZ = 5;

static const Vec3f BasisVectors[] = {
    Vec3f( 1.0f, 0.0f, 0.0f),
    Vec3f(-1.0f, 0.0f, 0.0f),
    Vec3f(0.0f,  1.0f, 0.0f),
    Vec3f(0.0f, -1.0f, 0.0f),
    Vec3f(0.0f, 0.0f,  1.0f),
    Vec3f(0.0f, 0.0f, -1.0f)
};

static const int ResU[] = {4, 3, 6, 1};
static const int ResV[] = {3, 4, 1, 6};

static const int OffsetU[][6] = {
    {2, 0, 1, 1, 1, 3},
    {1, 1, 1, 1, 0, 2},
    {0, 1, 2, 3, 4, 5},
    {0, 0, 0, 0, 0, 0},
};
static const int OffsetV[][6] = {
    {1, 1, 0, 2, 1, 1},
    {1, 3, 0, 2, 1, 1},
    {0, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5},
};
static const int BasisIndexU[][6] = {
    {NegZ, PosZ, PosX, PosX, PosX, NegX},
    {NegZ, NegZ, NegZ, NegZ, PosX, NegX},
    {NegZ, PosZ, PosX, PosX, PosX, NegX},
    {NegZ, PosZ, PosX, PosX, PosX, NegX},
};
static const int BasisIndexV[][6] = {
    {NegY, NegY, PosZ, NegZ, NegY, NegY},
    {NegY, PosY, PosX, NegX, NegY, NegY},
    {NegY, NegY, PosZ, NegZ, NegY, NegY},
    {NegY, NegY, PosZ, NegZ, NegY, NegY},
};

DEFINE_STRINGABLE_ENUM(CubemapCamera::ProjectionMode, "projection mode", ({
    {"horizontal_cross", CubemapCamera::MODE_HORIZONTAL_CROSS},
    {"vertical_cross", CubemapCamera::MODE_VERTICAL_CROSS},
    {"row", CubemapCamera::MODE_ROW},
    {"column", CubemapCamera::MODE_COLUMN}
}))

CubemapCamera::CubemapCamera()
: Camera(),
  _mode("horizontal_cross")
{
}

CubemapCamera::CubemapCamera(const Mat4f &transform, const Vec2u &res)
: Camera(transform, res),
  _mode("horizontal_cross")
{
}

inline void CubemapCamera::directionToFace(const Vec3f &d, int &face, Vec2f &offset) const
{
    int maxDim = std::abs(d).maxDim();
    face = d[maxDim] < 0.0f ? maxDim*2 + 1 : maxDim*2;
    offset = Vec2f(
        _basisU[face].dot(d)/std::abs(d[maxDim]),
        _basisV[face].dot(d)/std::abs(d[maxDim])
    )*0.5f + 0.5f;
}

inline Vec3f CubemapCamera::faceToDirection(int face, Vec2f offset) const
{
    Vec3f result = BasisVectors[face]
         + _basisU[face]*(offset.x()*2.0f - 1.0f)
         + _basisV[face]*(offset.y()*2.0f - 1.0f);
    return result.normalized();
}

inline Vec2f CubemapCamera::directionToUV(const Vec3f &wi, float &pdf) const
{
    int face;
    Vec2f offset;
    directionToFace(_invRot*wi, face, offset);
    float denom = 1.0f + sqr(offset.x()*2.0f - 1.0f) + sqr(offset.y()*2.0f - 1.0f);
    pdf = std::sqrt(denom*denom*denom)*(1.0f/24.0f);
    return _faceOffset[face] + offset*_faceSize;
}

inline bool CubemapCamera::uvToFace(Vec2f uv, int &face) const
{
    for (int i = 0; i < 6; ++i) {
        Vec2f delta = uv - _faceOffset[i];
        if (delta.x() >= 0.0f && delta.y() >= 0.0f && delta.x() <= _faceSize.x() && delta.y() <= _faceSize.y()) {
            face = i;
            return true;
        }
    }

    return false;
}

inline Vec3f CubemapCamera::uvToDirection(int face, Vec2f uv, float &pdf) const
{
    Vec2f delta = uv - _faceOffset[face];
    float denom = 1.0f + sqr(delta.x()*2.0f - 1.0f) + sqr(delta.y()*2.0f - 1.0f);
    pdf = std::sqrt(denom*denom*denom)*(1.0f/24.0f);

    return _rot*faceToDirection(face, delta/_faceSize);
}

void CubemapCamera::fromJson(JsonPtr value, const Scene &scene)
{
    Camera::fromJson(value, scene);
    _mode = value["mode"];
}

rapidjson::Value CubemapCamera::toJson(Allocator &allocator) const
{
    return JsonObject{Camera::toJson(allocator), allocator,
        "type", "cubemap",
        "mode", _mode.toString()
    };
}

bool CubemapCamera::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.weight = Vec3f(1.0f);
    sample.pdf = 1.0f;
    sample.Ng = _transform.fwd();

    return true;
}

bool CubemapCamera::sampleDirectionAndPixel(PathSampleGenerator &sampler, const PositionSample &point,
        Vec2u &pixel, DirectionSample &sample) const
{
    pixel = Vec2u(sampler.next2D()*Vec2f(_res));
    return sampleDirection(sampler, point, pixel, sample);
}

bool CubemapCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, Vec2u pixel,
        DirectionSample &sample) const
{
    Vec2f uv = (Vec2f(pixel) + 0.5f)*_pixelSize;

    int face;
    if (!uvToFace(uv, face))
        return false;

    float filterPdf;
    uv += _filter.sample(sampler.next2D(), filterPdf)*_pixelSize;

    sample.d = uvToDirection(face, uv, sample.pdf);
    sample.weight = Vec3f(1.0f);

    return true;
}

bool CubemapCamera::sampleDirect(const Vec3f &p, PathSampleGenerator &/*sampler*/, LensSample &sample) const
{
    sample.d = _pos - p;

    float pdf;
    Vec2f uv = directionToUV(-sample.d, pdf);

    sample.pixel = uv/_pixelSize;
    sample.weight = Vec3f(pdf*_visibleArea);

    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.weight /= rSq;

    return true;
}

bool CubemapCamera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    float pdf;
    Vec2f uv = directionToUV(direction.d, pdf);

    pixel = uv/_pixelSize;
    weight = Vec3f(pdf*_visibleArea);

    return true;
}

float CubemapCamera::directionPdf(const PositionSample &/*point*/, const DirectionSample &direction) const
{
    float pdf;
    directionToUV(direction.d, pdf);

    return pdf;
}

bool CubemapCamera::isDirac() const
{
    return true;
}

float CubemapCamera::approximateFov() const
{
    return 90.0f;
}

void CubemapCamera::prepareForRender()
{
    _rot = _transform.extractRotation();
    _invRot = _rot.transpose();

    int modeIdx = int(_mode);
    _faceSize = Vec2f(1.0f/ResU[modeIdx], 1.0f/ResV[modeIdx]);
    for (int i = 0; i < 6; ++i) {
        _faceOffset[i] = Vec2f(Vec2i(OffsetU[modeIdx][i], OffsetV[modeIdx][i]))*_faceSize;
        _basisU[i] = BasisVectors[BasisIndexU[modeIdx][i]];
        _basisV[i] = BasisVectors[BasisIndexV[modeIdx][i]];
    }

    _visibleArea = _res.x()*_res.y()*6.0f/(ResU[modeIdx]*ResV[modeIdx]);

    Camera::prepareForRender();
}

}
