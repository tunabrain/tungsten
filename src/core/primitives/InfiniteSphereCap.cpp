#include "InfiniteSphereCap.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

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
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Sphere", false, false);
    _proxy->makeCone(0.05f, 1.0f);
}

void InfiniteSphereCap::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    _scene = &scene;

    Primitive::fromJson(v, scene);
    JsonUtils::fromJson(v, "sample", _doSample);
    JsonUtils::fromJson(v, "skydome", _domeName);
    JsonUtils::fromJson(v, "cap_angle", _capAngleDeg);

    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));
}
rapidjson::Value InfiniteSphereCap::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "infinite_sphere_cap", allocator);
    v.AddMember("sample", _doSample, allocator);
    if (!_domeName.empty())
        v.AddMember("skydome", _domeName.c_str(), allocator);
    v.AddMember("cap_angle", _capAngleDeg, allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
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
    info.bsdf = _bsdf.get();
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
}

float InfiniteSphereCap::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    return SampleWarp::uniformSphericalCapPdf(_cosCapAngle);
}

bool InfiniteSphereCap::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    Vec3f dir = SampleWarp::uniformSphericalCap(sample.sampler->next2D(EmitterSample), _cosCapAngle);
    sample.d = _capFrame.toGlobal(dir);
    sample.dist = Ray::infinity();
    sample.pdf = SampleWarp::uniformSphericalCapPdf(_cosCapAngle);
    return true;
}

bool InfiniteSphereCap::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &/*sample*/) const
{
    return false;
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
}

void InfiniteSphereCap::teardownAfterRender()
{
}

int InfiniteSphereCap::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &InfiniteSphereCap::bsdf(int /*index*/)
{
    return _bsdf;
}

void InfiniteSphereCap::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *InfiniteSphereCap::clone()
{
    return new InfiniteSphereCap(*this);
}

}
