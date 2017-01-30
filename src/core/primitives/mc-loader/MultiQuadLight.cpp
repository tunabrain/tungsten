#include "MultiQuadLight.hpp"

#include "primitives/TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/LambertBsdf.hpp"

#include "Platform.hpp"
#include "Debug.hpp"

#include <iostream>

namespace Tungsten {
namespace MinecraftLoader {

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

static inline float quadSolidAngle(const Vec3f &p, const Vec3f &p0, const Vec3f &p1,
        const Vec3f &p2, const Vec3f &p3, const Vec3f &Ng)
{
    Vec3f R0 = p0 - p;

    if (R0.dot(Ng) >= 0.0f)
        return 0.0f;

    Vec3f R1 = p1 - p;
    Vec3f R2 = p2 - p;
    Vec3f R3 = p3 - p;
    Vec3f n0 = R0.cross(R1);
    Vec3f n1 = R1.cross(R2);
    Vec3f n2 = R2.cross(R3);
    Vec3f n3 = R3.cross(R0);
    float length0 = n0.length();
    float length1 = n1.length();
    float length2 = n2.length();
    float length3 = n3.length();
    if (length0 == 0.0f || length1 == 0.0f || length2 == 0.0f || length3 == 0.0f)
        return 0.0f;
    float Q =
          std::acos(clamp(n0.dot(n1)/(length0*length1), -1.0f, 1.0f))
        + std::acos(clamp(n1.dot(n2)/(length1*length2), -1.0f, 1.0f))
        + std::acos(clamp(n2.dot(n3)/(length2*length3), -1.0f, 1.0f))
        + std::acos(clamp(n3.dot(n0)/(length3*length0), -1.0f, 1.0f));

    return max(TWO_PI - std::abs(Q), 0.0f);
}

static inline float rsqrt_fast(const float x)
{
  const __m128 a = _mm_set_ss(x);
  const __m128 r = _mm_rsqrt_ps(a);
  return _mm_cvtss_f32(r);
}

FORCE_INLINE float MultiQuadLight::approximateQuadContribution(const Vec3f &p, uint32 quad) const
{
    const PrecomputedQuad &q = _precomputedQuads[quad];

    Vec3f d = p - q.center;
    float cosTheta = q.Ngu.dot(d);
    if (cosTheta <= 0.0f)
        return 0.0f;

    float rSq = d.lengthSq();
    float weight;
    if (rSq < 1.0f) {
        const QuadGeometry::TriangleInfo &t1 = _geometry.triangle(quad*2);
        const QuadGeometry::TriangleInfo &t2 = _geometry.triangle(quad*2 + 1);
        weight = quadSolidAngle(p, t1.p0, t1.p2, t1.p1, t2.p0, t1.Ng)*
                _materials[t1.material].sampleWeight;
    } else {
        float ir = rsqrt_fast(rSq);
        weight = cosTheta*ir*ir*ir;
    }

    return weight;
}

void MultiQuadLight::constructSampleBounds()
{
    int quadCount = _geometry.triangleCount()/2;

    Bvh::PrimVector samplePrims;
    std::vector<float> weights;

    samplePrims.reserve(quadCount);
    weights.reserve(quadCount);
    _precomputedQuads.reserve(quadCount);

    const float SampleThreshold = 0.1f;
    for (int i = 0; i < _geometry.triangleCount(); i += 2) {
        const QuadGeometry::TriangleInfo &t1 = _geometry.triangle(i);
        const QuadGeometry::TriangleInfo &t2 = _geometry.triangle(i + 1);

        Vec3f center = (t1.p0 + t1.p1 + t1.p2 + t2.p0)/4.0f;
        float emissionWeight = _materials[t1.material].sampleWeight;
        emissionWeight *=
                  MathUtil::triangleArea(t1.p0, t1.p1, t1.p2)
                + MathUtil::triangleArea(t2.p0, t2.p1, t2.p2);

        float radius = std::sqrt((emissionWeight*0.5f)/SampleThreshold);

        Box3f bounds(center + 0.5f*radius*t1.Ng);
        bounds.grow((0.5f*radius + (center - t1.p0).length())/std::sqrt(3.0f));

        float contributionAtBounds = _materials[t1.material].sampleWeight*
                quadSolidAngle(center + radius*t1.Ng, t1.p0, t1.p2, t1.p1, t2.p0, t1.Ng);

        _precomputedQuads.emplace_back(PrecomputedQuad{center, t1.Ng*emissionWeight});
        weights.push_back(radius*radius*contributionAtBounds);
        samplePrims.emplace_back(bounds, bounds.center(), i/2);
    }

    _sampleBvh.reset(new SolidAngleBvh(std::move(samplePrims), std::move(weights)));
}

void MultiQuadLight::fromJson(JsonPtr /*value*/, const Scene &/*scene*/)
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

    _bvh->trace(ray, [&](Ray &ray, uint32 idx, float /*tMin*/, const Vec3pf &/*bounds*/) {
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
    info.bsdf = _materials[_geometry.material(isect->isect)].emitterBsdf.get();
    info.primitive = this;
}

bool MultiQuadLight::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool MultiQuadLight::isSamplable() const
{
    return true;
}

void MultiQuadLight::makeSamplable(const TraceableScene &/*scene*/, uint32 threadIndex)
{
    _samplers.resize(threadIndex + 1);
    _samplers[threadIndex].reset(new ThreadlocalSampleInfo);
    _samplers[threadIndex]->sampleWeights.resize(_geometry.triangleCount());
    _samplers[threadIndex]->insideIds.resize(_geometry.triangleCount());
    _samplers[threadIndex]->lastQuery = Vec3f(0.0f);
}

bool MultiQuadLight::sampleDirect(uint32 threadIndex, const Vec3f &p, PathSampleGenerator &pathSampler, LightSample &sample) const
{
    float xi = pathSampler.next1D();

    ThreadlocalSampleInfo &sampler = *_samplers[threadIndex];

    std::pair<int, float> result = _sampleBvh->sampleLight(p, &sampler.sampleWeights[0],
            &sampler.insideIds[0], xi, [&](uint32 id) {
        return approximateQuadContribution(p, id);
    });
    if (result.first == -1)
        return false;

    int idx = pathSampler.next1D() < 0.5f ? result.first*2 : result.first*2 + 1;

    const QuadGeometry::TriangleInfo &t = _geometry.triangle(idx);

    Vec3f q = SampleWarp::uniformTriangle(pathSampler.next2D(), t.p0, t.p1, t.p2);
    Vec3f L = q - p;

    float rSq = L.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d = L/sample.dist;
    float cosTheta = -(t.Ng.dot(sample.d));
    if (cosTheta <= 0.0f)
        return false;
    sample.pdf = result.second*0.5f*rSq/(cosTheta*_triangleAreas[idx]);

    return true;
}

float MultiQuadLight::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &data,
        const IntersectionInfo &info, const Vec3f &p) const
{
    const QuadLightIntersection *isect = data.as<QuadLightIntersection>();

    uint32 quadId = isect->isect.id/2;

    float pdf = _sampleBvh->lightPdf(p, quadId, [&](uint32 id) {
        return approximateQuadContribution(p, id);
    });

    return pdf*0.5f*(p - info.p).lengthSq()/(-info.w.dot(info.Ng)*_triangleAreas[isect->isect.id]);
}

Vec3f MultiQuadLight::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    const QuadLightIntersection *isect = data.as<QuadLightIntersection>();
    const QuadMaterial &material = _materials[_geometry.material(isect->isect)];
    const BitmapTexture *emitter = material.emission.get();

    return (*emitter)[info.uv]*(isect->wasPrimary ? material.primaryScale : material.secondaryScale);
}

bool MultiQuadLight::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool MultiQuadLight::isDirac() const
{
    return false;
}

bool MultiQuadLight::isInfinite() const
{
    return false;
}

float MultiQuadLight::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    return _sampleBvh->approximateContribution(p, [&](uint32 id) {
        return approximateQuadContribution(p, id);
    })*(1.0f/6.0f);
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

void MultiQuadLight::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &/*bsdf*/)
{
    FAIL("MultiQuadLight::setBsdf should never be called");
}

void MultiQuadLight::prepareForRender()
{
    Bvh::PrimVector prims;
    prims.reserve(_geometry.size());

    _bounds = Box3f();

    for (int i = 0; i < _geometry.size(); ++i) {
        Box3f bounds = _geometry.bounds(i);
        _bounds.grow(bounds);

        prims.emplace_back(bounds, bounds.center(), i);
    }

    _triangleAreas.resize(_geometry.triangleCount());
    for (int i = 0; i < _geometry.triangleCount(); ++i) {
        const QuadGeometry::TriangleInfo &t = _geometry.triangle(i);
        _triangleAreas[i] = MathUtil::triangleArea(t.p0, t.p1, t.p2);
    }

    _bvh.reset(new Bvh::BinaryBvh(std::move(prims), 1));

    constructSampleBounds();
}

void MultiQuadLight::teardownAfterRender()
{
    _samplers.clear();
    _bvh.reset();
    _sampleBvh.reset();
    _precomputedQuads.clear();
    _triangleAreas.clear();
}

Primitive *MultiQuadLight::clone()
{
    return nullptr;
}

bool MultiQuadLight::isEmissive() const
{
    return true;
}

}
}
