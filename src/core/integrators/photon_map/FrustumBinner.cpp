#include "FrustumBinner.hpp"

namespace Tungsten {

FrustumBinner::FrustumBinner(const Camera &camera)
: _guardBand(3 + TileSize*1.41421f + 2.0f),
  _res(camera.resolution()),
  _aspect(_res.x()/float(_res.y())),
  _f(1.0f/std::tan(camera.approximateFov()*0.5f)),
  _scale(Vec2f(_f, -_f*_aspect)*0.5f),
  _invT(camera.invTransform()),
  _pos(camera.pos())
{
}

static inline Vec3f triangleSetup(const Vec2f &a, const Vec2f &b, float band)
{
    float l = (b - a).length()*band + 1.0f;
    return Vec3f(
        a.y() - b.y(),
        b.x() - a.x(),
        (b.y() - a.y())*a.x() - (b.x() - a.x())*a.y() + l
    );
}

static inline float orient2d(const Vec2f &a, const Vec2f &b, const Vec2f &c)
{
    return (b.x() - a.x())*(c.y() - a.y()) - (b.y() - a.y())*(c.x() - a.x());
}

template<typename Vec>
void clip(const Vec *in, int inCount, Vec *out, int &outCount, Vec plane, float offs)
{
    outCount = 0;
    for (int i = 0; i < inCount; ++i) {
        Vec p0 = in[(i + 0) % inCount];
        Vec p1 = in[(i + 1) % inCount];
        float u0 = p0.dot(plane);
        float u1 = p1.dot(plane);
        bool clip0 = u0 < offs;
        bool clip1 = u1 < offs;
        if (clip0 && clip1)
            continue;
        if (clip0)
            p0 += (p1 - p0)*(offs - plane.dot(p0))/(plane.dot(p1 - p0));
        else if (clip1)
            p1 += (p0 - p1)*(offs - plane.dot(p1))/(plane.dot(p0 - p1));

        out[outCount++] = p0;
        if (clip1)
            out[outCount++] = p1;
    }
}

FrustumBinner::QuadSetup FrustumBinner::setupQuad(Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3) const
{
    Vec3f vertices[] = {p0, p1, p2, p3};
    Vec3f clippedVerts[5];
    int clippedCount = 0;
    const float eps = 1e-5f;
    clip(vertices, 4, clippedVerts, clippedCount, Vec3f(0.0f, 0.0f, 1.0f), eps);

    Vec2f screenVertsA[10], screenVertsB[10];
    for (int j = 0; j < clippedCount; ++j)
        screenVertsA[j] = (clippedVerts[j].xy()/std::abs(clippedVerts[j].z())*_scale + 0.5f);

    int clippedCountA = clippedCount, clippedCountB;
    clip(screenVertsA, clippedCountA, screenVertsB, clippedCountB, Vec2f( 1.0f,  0.0f), 0.0f);
    clip(screenVertsB, clippedCountB, screenVertsA, clippedCountA, Vec2f( 0.0f,  1.0f), 0.0f);
    clip(screenVertsA, clippedCountA, screenVertsB, clippedCountB, Vec2f(-1.0f,  0.0f), -1.0f);
    clip(screenVertsB, clippedCountB, screenVertsA, clippedCountA, Vec2f( 0.0f, -1.0f), -1.0f);

    Box2f bounds;
    for (int i = 0; i < clippedCountA; ++i) {
        screenVertsA[i] *= Vec2f(_res);
        bounds.grow(screenVertsA[i]);
    }
    screenVertsA[clippedCountA] = screenVertsA[0];
    bounds.grow(_guardBand);

    int minX = max(int(bounds.min().x()), 0), maxX = min(int(bounds.max().x() + 1.0f), int(_res.x()));
    int minY = max(int(bounds.min().y()), 0), maxY = min(int(bounds.max().y() + 1.0f), int(_res.y()));

    float sign = orient2d(screenVertsA[0], screenVertsA[1], screenVertsA[2]) < 0.0f ? -1.0f : 1.0f;

    Vec3f setups[12];
    if (clippedCountA == 0) {
        for (int j = 0; j < 12; ++j)
            setups[j] = Vec3f(0.0f, 0.0f, -1.0f);
    } else {
        for (int j = 0; j < 12; ++j)
            setups[j] = Vec3f(0.0f);
        for (int j = 0; j < clippedCountA; ++j)
            setups[j] = triangleSetup(screenVertsA[j], screenVertsA[j + 1], _guardBand*sign)*sign;
    }

    QuadSetup result;
    result.bounds = Box2i(Vec2i(minX, minY), Vec2i(maxX, maxY));
    for (int j = 0; j < 12; ++j) {
        result.stepX [j/4][j % 4] = setups[j].x();
        result.stepY [j/4][j % 4] = setups[j].y();
        result.offset[j/4][j % 4] = setups[j].z();
    }
    return result;
}

}
