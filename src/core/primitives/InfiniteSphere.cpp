#include "InfiniteSphere.hpp"
#include "TriangleMesh.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

struct InfiniteSphereIntersection
{
    Vec3f p;
    Vec3f w;
};

InfiniteSphere::InfiniteSphere()
: _doSample(true)
{
}

Vec2f InfiniteSphere::directionToUV(const Vec3f &wi) const
{
    Vec3f wLocal = _invRotTransform*wi;
    return Vec2f(std::atan2(wLocal.z(), wLocal.x())*INV_TWO_PI + 0.5f, std::acos(-wLocal.y())*INV_PI);
}

Vec2f InfiniteSphere::directionToUV(const Vec3f &wi, float &sinTheta) const
{
    Vec3f wLocal = _invRotTransform*wi;
    sinTheta = std::sqrt(max(1.0f - wLocal.y()*wLocal.y(), 0.0f));
    return Vec2f(std::atan2(wLocal.z(), wLocal.x())*INV_TWO_PI + 0.5f, std::acos(-wLocal.y())*INV_PI);
}

Vec3f InfiniteSphere::uvToDirection(Vec2f uv, float &sinTheta) const
{
    float phi   = (uv.x() - 0.5f)*TWO_PI;
    float theta = uv.y()*PI;
    sinTheta = std::sin(theta);
    return _rotTransform*Vec3f(
        std::cos(phi)*sinTheta,
        -std::cos(theta),
        std::sin(phi)*sinTheta
    );
}

void InfiniteSphere::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Sphere", false, false);
    _proxy->makeSphere(0.05f);
}

void InfiniteSphere::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
    JsonUtils::fromJson(v, "sample", _doSample);

    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));
}
rapidjson::Value InfiniteSphere::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "infinite_sphere", allocator);
    v.AddMember("sample", _doSample, allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
}

bool InfiniteSphere::intersect(Ray &ray, IntersectionTemporary &data) const
{
    InfiniteSphereIntersection *isect = data.as<InfiniteSphereIntersection>();
    isect->p = ray.pos();
    isect->w = ray.dir();
    data.primitive = this;
    return true;
}

bool InfiniteSphere::occluded(const Ray &/*ray*/) const
{
    return true;
}

bool InfiniteSphere::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void InfiniteSphere::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const InfiniteSphereIntersection *isect = data.as<InfiniteSphereIntersection>();
    info.Ng = info.Ns = -isect->w;
    info.p = isect->p;
    info.uv = directionToUV(isect->w);
    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool InfiniteSphere::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool InfiniteSphere::isSamplable() const
{
    return _doSample;
}

void InfiniteSphere::makeSamplable(uint32 /*threadIndex*/)
{
    _emission->makeSamplable(MAP_SPHERICAL);
}

float InfiniteSphere::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &data, const IntersectionInfo &/*info*/,
        const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    if (_emission->isConstant()) {
        return INV_FOUR_PI;
    } else {
        const InfiniteSphereIntersection *isect = data.as<InfiniteSphereIntersection>();
        float sinTheta;
        Vec2f uv = directionToUV(isect->w, sinTheta);
        return INV_PI*INV_TWO_PI*_emission->pdf(MAP_SPHERICAL, uv)/sinTheta;
    }
}

bool InfiniteSphere::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    if (_emission->isConstant()) {
        sample.d = SampleWarp::uniformSphere(sample.sampler->next2D());
        sample.dist = 1e30f;
        sample.pdf = INV_FOUR_PI;
        return true;
    } else {
        Vec2f uv = _emission->sample(MAP_SPHERICAL, sample.sampler->next2D());
        float sinTheta;
        sample.d = uvToDirection(uv, sinTheta);
        sample.pdf = INV_PI*INV_TWO_PI*_emission->pdf(MAP_SPHERICAL, uv)/sinTheta;
        sample.dist = 1e30f;
        return true;
    }
}

bool InfiniteSphere::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &/*sample*/) const
{
    return false;
}

bool InfiniteSphere::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool InfiniteSphere::isDelta() const
{
    return false;
}

bool InfiniteSphere::isInfinite() const
{
    return true;
}

float InfiniteSphere::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    if (!isEmissive() || !isSamplable())
        return 0.0f;
    return TWO_PI*_emission->average().max();
}

Box3f InfiniteSphere::bounds() const
{
    return Box3f(Vec3f(-1e30f), Vec3f(1e30f));
}

const TriangleMesh &InfiniteSphere::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void InfiniteSphere::prepareForRender()
{
    _rotTransform = _transform.extractRotation();
    _invRotTransform = _rotTransform.transpose();
}

void InfiniteSphere::cleanupAfterRender()
{
}

int InfiniteSphere::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &InfiniteSphere::bsdf(int /*index*/)
{
    return _bsdf;
}

Primitive *InfiniteSphere::clone()
{
    return new InfiniteSphere(*this);
}

}
