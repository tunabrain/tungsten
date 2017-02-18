#include "InfiniteSphereCap.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/NullBsdf.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

struct InfiniteSphereCapIntersection
{
    Vec3f p;
    Vec3f w;
};

InfiniteSphereCap::InfiniteSphereCap()
: _scene(nullptr),
  _doSample(true),
  _capAngleDeg(10.0f)
{
}

void InfiniteSphereCap::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(),
            std::make_shared<NullBsdf>(), "Sphere", false, false);
    _proxy->makeCone(0.05f, 1.0f);
}

float InfiniteSphereCap::powerToRadianceFactor() const
{
    return INV_TWO_PI/(1.0f - _cosCapAngle);
}

void InfiniteSphereCap::fromJson(JsonPtr value, const Scene &scene)
{
    _scene = &scene;

    Primitive::fromJson(value, scene);
    value.getField("sample", _doSample);
    value.getField("skydome", _domeName);
    value.getField("cap_angle", _capAngleDeg);
}
rapidjson::Value InfiniteSphereCap::toJson(Allocator &allocator) const
{
    JsonObject result{Primitive::toJson(allocator), allocator,
        "type", "infinite_sphere_cap",
        "sample", _doSample,
        "cap_angle", _capAngleDeg
    };
    if (!_domeName.empty())
        result.add("skydome", _domeName);

    return result;
}

bool InfiniteSphereCap::intersect(Ray &ray, IntersectionTemporary &data) const
{
    if (ray.dir().dot(_capDir) < _cosCapAngle)
        return false;

    InfiniteSphereCapIntersection *isect = data.as<InfiniteSphereCapIntersection>();
    isect->p = ray.pos();
    isect->w = ray.dir();
    data.primitive = this;
    return true;
}

bool InfiniteSphereCap::occluded(const Ray &ray) const
{
    return ray.dir().dot(_capDir) >= _cosCapAngle;
}

bool InfiniteSphereCap::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void InfiniteSphereCap::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const InfiniteSphereCapIntersection *isect = data.as<InfiniteSphereCapIntersection>();
    info.Ng = info.Ns = -isect->w;
    info.p = isect->p;
    info.uv = Vec2f(0.0f, 0.0f);
    info.primitive = this;
    info.bsdf = nullptr;
}

bool InfiniteSphereCap::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/, Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool InfiniteSphereCap::isSamplable() const
{
    return _doSample;
}

void InfiniteSphereCap::makeSamplable(const TraceableScene &scene, uint32 /*threadIndex*/)
{
    _sceneBounds = scene.bounds();
    _sceneBounds.grow(1e-2f);
}

bool InfiniteSphereCap::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    sample.uv = Vec2f(0.0f);
    sample.Ng = -_capFrame.toGlobal(SampleWarp::uniformSphericalCap(sampler.next2D(), _cosCapAngle));

    float faceXi = sampler.next1D();
    Vec2f xi = sampler.next2D();
    sample.p = SampleWarp::projectedBox(_sceneBounds, sample.Ng, faceXi, xi);
    sample.pdf = SampleWarp::projectedBoxPdf(_sceneBounds, sample.Ng);
    sample.weight = Vec3f(1.0f/sample.pdf);

    return true;
}

bool InfiniteSphereCap::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &point, DirectionSample &sample) const
{
    sample.d = point.Ng;
    sample.pdf = SampleWarp::uniformSphericalCapPdf(_cosCapAngle);
    sample.weight = (*_emission)[point.uv]/sample.pdf;

    return true;
}

bool InfiniteSphereCap::sampleDirect(uint32 /*threadIndex*/, const Vec3f &/*p*/, PathSampleGenerator &sampler, LightSample &sample) const
{
    Vec3f dir = SampleWarp::uniformSphericalCap(sampler.next2D(), _cosCapAngle);
    sample.d = _capFrame.toGlobal(dir);
    sample.dist = Ray::infinity();
    sample.pdf = SampleWarp::uniformSphericalCapPdf(_cosCapAngle);

    return true;
}

bool InfiniteSphereCap::invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const
{
    float faceXi;
    Vec2f xi;
    if (!SampleWarp::invertProjectedBox(_sceneBounds, point.p, -point.Ng, faceXi, xi, sampler.untracked1D()))
        return false;

    sampler.put1D(faceXi);
    sampler.put2D(xi);

    return true;
}

bool InfiniteSphereCap::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    Vec3f d = -_capFrame.toLocal(direction.d);
    Vec2f xi;
    if (!SampleWarp::invertUniformSphericalCap(d, _cosCapAngle, xi, sampler.untracked1D()))
        return false;

    sampler.put2D(xi);

    return true;
}

float InfiniteSphereCap::positionalPdf(const PositionSample &point) const
{
    return SampleWarp::projectedBoxPdf(_sceneBounds, point.Ng);
}

float InfiniteSphereCap::directionalPdf(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return SampleWarp::uniformSphericalCapPdf(_cosCapAngle);
}

float InfiniteSphereCap::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/) const
{
    return SampleWarp::uniformSphericalCapPdf(_cosCapAngle);
}

Vec3f InfiniteSphereCap::evalPositionalEmission(const PositionSample &/*sample*/) const
{
    return Vec3f(1.0f);
}

Vec3f InfiniteSphereCap::evalDirectionalEmission(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return (*_emission)[Vec2f(0.0f)];
}

Vec3f InfiniteSphereCap::evalDirect(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/) const
{
    return (*_emission)[Vec2f(0.0f)];
}

bool InfiniteSphereCap::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool InfiniteSphereCap::isDirac() const
{
    return false;
}

bool InfiniteSphereCap::isInfinite() const
{
    return true;
}

float InfiniteSphereCap::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    if (!isEmissive() || !isSamplable())
        return 0.0f;
    return TWO_PI*(1.0f - _cosCapAngle)*_emission->average().max();
}

Box3f InfiniteSphereCap::bounds() const
{
    return Box3f(Vec3f(-1e30f), Vec3f(1e30f));
}

const TriangleMesh &InfiniteSphereCap::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void InfiniteSphereCap::prepareForRender()
{
    Mat4f tform = _transform;
    if (!_domeName.empty()) {
        const Primitive *prim = _scene->findPrimitive(_domeName);
        if (!prim)
            DBG("Note: unable to find pivot object '%s' for infinity sphere cap", _domeName.c_str());
        else
            tform = prim->transform();
    }

    _capDir = tform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();
    _capAngleRad = Angle::degToRad(_capAngleDeg);
    _cosCapAngle = std::cos(_capAngleRad);
    _capFrame = TangentFrame(_capDir);

    Primitive::prepareForRender();
}

int InfiniteSphereCap::numBsdfs() const
{
    return 0;
}

std::shared_ptr<Bsdf> &InfiniteSphereCap::bsdf(int /*index*/)
{
    FAIL("InfiniteSphereCap::bsdf should not be called");
}

void InfiniteSphereCap::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &/*bsdf*/)
{
}

Primitive *InfiniteSphereCap::clone()
{
    return new InfiniteSphereCap(*this);
}

}
