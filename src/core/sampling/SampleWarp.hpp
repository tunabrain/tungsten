#ifndef SAMPLE_HPP_
#define SAMPLE_HPP_

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"
#include "math/Box.hpp"

#include <algorithm>
#include <cmath>

namespace Tungsten {

namespace SampleWarp {

static inline Vec3f uniformHemisphere(const Vec2f &xi)
{
    float phi  = TWO_PI*xi.x();
    float r = std::sqrt(max(1.0f - xi.y()*xi.y(), 0.0f));
    return Vec3f(std::cos(phi)*r, std::sin(phi)*r, xi.y());
}

static inline float uniformHemispherePdf(const Vec3f &/*p*/)
{
    return INV_TWO_PI;
}

static inline Vec3f cosineHemisphere(const Vec2f &xi)
{
    float phi = xi.x()*TWO_PI;
    float r = std::sqrt(xi.y());

    return Vec3f(
            std::cos(phi)*r,
            std::sin(phi)*r,
            std::sqrt(max(1.0f - xi.y(), 0.0f))
    );
}

static inline float cosineHemispherePdf(const Vec3f &p)
{
    return p.z()*INV_PI;
}

static inline Vec3f uniformDisk(const Vec2f &xi)
{
    float phi = xi.x()*TWO_PI;
    float r = std::sqrt(xi.y());
    return Vec3f(std::cos(phi)*r, std::sin(phi)*r, 0.0f);
}

static inline float uniformDiskPdf()
{
    return INV_PI;
}

static inline Vec3f uniformCylinder(Vec2f &xi)
{
    float phi = xi.x()*TWO_PI;
    return Vec3f(
        std::sin(phi),
        std::cos(phi),
        xi.y()*2.0f - 1.0f
    );
}

static inline float uniformCylinderPdf()
{
    return INV_PI;
}

static inline Vec3f uniformSphere(const Vec2f &xi)
{
    float phi = xi.x()*TWO_PI;
    float z = xi.y()*2.0f - 1.0f;
    float r = std::sqrt(max(1.0f - z*z, 0.0f));

    return Vec3f(
        std::cos(phi)*r,
        std::sin(phi)*r,
        z
    );
}

static inline float uniformSpherePdf()
{
    return INV_FOUR_PI;
}

static inline Vec3f uniformSphericalCap(const Vec2f &xi, float cosThetaMax)
{
    float phi = xi.x()*TWO_PI;
    float z = xi.y()*(1.0f - cosThetaMax) + cosThetaMax;
    float r = std::sqrt(max(1.0f - z*z, 0.0f));
    return Vec3f(
        std::sin(phi)*r,
        std::cos(phi)*r,
        z
    );
}

static inline float uniformSphericalCapPdf(float cosThetaMax)
{
    return INV_TWO_PI/(1.0f - cosThetaMax);
}

static inline Vec3f phongHemisphere(const Vec2f &xi, int n)
{
    float phi = xi.x()*TWO_PI;
    float cosTheta = std::pow(xi.y(), 1.0f/(n + 1.0f));
    float r = std::sqrt(std::max(1.0f - cosTheta*cosTheta, 0.0f));
    return Vec3f(std::sin(phi)*r, std::cos(phi)*r, cosTheta);
}

static inline float phongHemispherePdf(const Vec3f &v, int n)
{
    return INV_TWO_PI*(n + 1.0f)*std::pow(v.z(), n);
}

static inline Vec2f uniformTriangleUv(const Vec2f &xi)
{
    float uSqrt = std::sqrt(xi.x());
    float alpha = 1.0f - uSqrt;
    float beta = (1.0f - xi.y())*uSqrt;

    return Vec2f(alpha, beta);
}

static inline Vec3f uniformTriangle(const Vec2f &xi, const Vec3f& a, const Vec3f& b, const Vec3f& c)
{
    Vec2f uv = uniformTriangleUv(xi);
    return a*uv.x() + b*uv.y() + c*(1.0f - uv.x() - uv.y());
}

static inline float uniformTrianglePdf(const Vec3f &a, const Vec3f &b, const Vec3f &c)
{
    return 2.0f/((b - a).cross(c - a).length());
}

static inline float powerHeuristic(float pdf0, float pdf1)
{
    return (pdf0*pdf0)/(pdf0*pdf0 + pdf1*pdf1);
}

static inline Vec3f projectedBox(const Box3f &box, const Vec3f &direction, float faceXi, const Vec2f &xi)
{
    Vec3f diag = box.diagonal();
    float areaX = diag.y()*diag.z()*std::abs(direction.x());
    float areaY = diag.z()*diag.x()*std::abs(direction.y());
    float areaZ = diag.x()*diag.y()*std::abs(direction.z());

    float u = faceXi*(areaX + areaY + areaZ);

    Vec3f result;
    if (u < areaX) {
        result.x() = direction.x() < 0.0f ? box.max().x() : box.min().x();
        result.y() = box.min().y() + diag.y()*xi.x();
        result.z() = box.min().z() + diag.z()*xi.y();
    } else if (u < areaX + areaY) {
        result.y() = direction.y() < 0.0f ? box.max().y() : box.min().y();
        result.x() = box.min().x() + diag.x()*xi.x();
        result.z() = box.min().z() + diag.z()*xi.y();
    } else {
        result.z() = direction.z() < 0.0f ? box.max().z() : box.min().z();
        result.x() = box.min().x() + diag.x()*xi.x();
        result.y() = box.min().y() + diag.y()*xi.y();
    }

    return result;
}

static inline float projectedBoxPdf(const Box3f &box, const Vec3f &direction)
{
    Vec3f diag = box.diagonal();
    float areaX = diag.y()*diag.z()*std::abs(direction.x());
    float areaY = diag.z()*diag.x()*std::abs(direction.y());
    float areaZ = diag.x()*diag.y()*std::abs(direction.z());

    return 1.0f/(areaX + areaY + areaZ);
}

}

}

#endif /* SAMPLE_HPP_ */
