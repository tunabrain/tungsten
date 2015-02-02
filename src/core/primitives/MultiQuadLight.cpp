#include "MultiQuadLight.hpp"
#include "TriangleMesh.hpp"

#include "bsdfs/LambertBsdf.hpp"

#include <iostream>

namespace Tungsten {

struct QuadLightIntersection
{
    QuadGeometry::Intersection isect;
    bool wasPrimary;
};

MultiQuadLight::MultiQuadLight(QuadGeometry geometry, const std::vector<QuadMaterial> &materials)
: _geometry(std::move(geometry)),
  _materials(materials)
{
}

void MultiQuadLight::fromJson(const rapidjson::Value &/*v*/, const Scene &/*scene*/)
{
}

rapidjson::Value MultiQuadLight::toJson(Allocator &allocator) const
{
    return Primitive::toJson(allocator);
}

bool MultiQuadLight::intersect(Ray &ray, IntersectionTemporary &data) const
{
    QuadLightIntersection *isect = data.as<QuadLightIntersection>();
    isect->wasPrimary = ray.isPrimaryRay();

    float farT = ray.farT();

    _bvh->trace(ray, [&](Ray &ray, uint32 idx, float /*tMin*/) {
        _geometry.intersect(ray, idx, isect->isect);
    });

    if (ray.farT() < farT) {
        data.primitive = this;
        return true;
    }

    return false;
}

bool MultiQuadLight::occluded(const Ray &ray) const
{
    IntersectionTemporary data;
    Ray temp(ray);
    return intersect(temp, data);
}

bool MultiQuadLight::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void MultiQuadLight::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const QuadLightIntersection *isect = data.as<QuadLightIntersection>();

    info.Ng = info.Ns = _geometry.normal(isect->isect);
    info.uv = _geometry.uv(isect->isect);
    info.bsdf = _materials[_geometry.material(isect->isect)].bsdf.get();
    info.primitive = this;
}

bool MultiQuadLight::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool MultiQuadLight::isSamplable() const
{
    return false; /* TODO */
}

void MultiQuadLight::makeSamplable(uint32 /*threadIndex*/)
{
    /* TODO */
}

float MultiQuadLight::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    return 0.0f; /* TODO */
}

bool MultiQuadLight::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &/*sample*/) const
{
    return false; /* TODO */
}

bool MultiQuadLight::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &/*sample*/) const
{
    return false;
}

bool MultiQuadLight::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool MultiQuadLight::isDelta() const
{
    return false;
}

bool MultiQuadLight::isInfinite() const
{
    return false;
}

float MultiQuadLight::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return -1.0f; /* TODO */
}

Box3f MultiQuadLight::bounds() const
{
    return _bounds;
}

const TriangleMesh &MultiQuadLight::asTriangleMesh()
{
    if (!_proxy) {
        std::vector<Vertex> verts;
        std::vector<TriangleI> tris;

        for (int i = 0; i < _geometry.triangleCount(); ++i) {
            const QuadGeometry::TriangleInfo &info = _geometry.triangle(i);

            verts.emplace_back(info.p0);
            verts.emplace_back(info.p1);
            verts.emplace_back(info.p2);
            tris.emplace_back(i*3, i*3 + 1, i*3 + 2);
        }

        _proxy.reset(new TriangleMesh(verts, tris, std::make_shared<LambertBsdf>(), "", false, false));
    }
    return *_proxy;
}

int MultiQuadLight::numBsdfs() const
{
    return 0;
}

std::shared_ptr<Bsdf> &MultiQuadLight::bsdf(int /*index*/)
{
    FAIL("MultiQuadLight::bsdf should never be called");
}

void MultiQuadLight::prepareForRender()
{
    Bvh::PrimVector prims;

    _bounds = Box3f();

    for (int i = 0; i < _geometry.size(); ++i) {
        Box3f bounds = _geometry.bounds(i);
        _bounds.grow(bounds);

        prims.emplace_back(bounds, bounds.center(), i);
    }

    _bvh.reset(new Bvh::BinaryBvh(std::move(prims), 1));
}

void MultiQuadLight::cleanupAfterRender()
{
    _bvh.reset();
}

Primitive *MultiQuadLight::clone()
{
    return nullptr;
}

bool MultiQuadLight::isEmissive() const
{
    return true;
}

Vec3f MultiQuadLight::emission(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    const QuadLightIntersection *isect = data.as<QuadLightIntersection>();

    const QuadMaterial &material = _materials[_geometry.material(isect->isect)];

    const BitmapTexture *emitter = material.emission.get();

    if (emitter) {
        if (isect->wasPrimary)
            return (*emitter)[info.uv];
        else
            return material.emissionColor;
    } else {
        return Vec3f(0.0f);
    }
}

}
