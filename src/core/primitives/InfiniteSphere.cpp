#include "InfiniteSphere.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/NullBsdf.hpp"

#include "math/Angle.hpp"

#include "io/JsonObject.hpp"
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
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(),
            std::make_shared<NullBsdf>(), "Sphere", false, false);
    _proxy->makeSphere(0.05f);
}

float InfiniteSphere::powerToRadianceFactor() const
{
    return INV_FOUR_PI;
}

void InfiniteSphere::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);
    value.getField("sample", _doSample);
}
rapidjson::Value InfiniteSphere::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "infinite_sphere",
        "sample", _doSample
    };
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
    info.bsdf = nullptr;
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

void InfiniteSphere::makeSamplable(const TraceableScene &scene, uint32 /*threadIndex*/)
{
    _emission->makeSamplable(MAP_SPHERICAL);
    _sceneBounds = scene.bounds();
    _sceneBounds.grow(1e-2f);
}

bool InfiniteSphere::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    if (_emission->isConstant()) {
        sample.Ng = -SampleWarp::uniformSphere(sampler.next2D());
        sample.uv = directionToUV(-sample.Ng);
    } else {
        sample.uv = _emission->sample(MAP_SPHERICAL, sampler.next2D());
        float sinTheta;
        sample.Ng = -uvToDirection(sample.uv, sinTheta);
    }

    float faceXi = sampler.next1D();
    Vec2f xi = sampler.next2D();
    sample.p = SampleWarp::projectedBox(_sceneBounds, sample.Ng, faceXi, xi);
    sample.pdf = SampleWarp::projectedBoxPdf(_sceneBounds, sample.Ng);
    sample.weight = Vec3f(1.0f/sample.pdf);

    return true;
}

bool InfiniteSphere::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &point, DirectionSample &sample) const
{
    sample.d = point.Ng;
    if (_emission->isConstant()) {
        sample.pdf = INV_FOUR_PI;
    } else {
        float sinTheta;
        directionToUV(-point.Ng, sinTheta);
        sample.pdf = INV_PI*INV_TWO_PI*_emission->pdf(MAP_SPHERICAL, point.uv)/sinTheta;
        if (sample.pdf == 0.0f)
            return false;
    }
    sample.weight = (*_emission)[point.uv]/sample.pdf;

    return true;
}

bool InfiniteSphere::sampleDirect(uint32 /*threadIndex*/, const Vec3f &/*p*/, PathSampleGenerator &sampler, LightSample &sample) const
{
    if (_emission->isConstant()) {
        sample.d = SampleWarp::uniformSphere(sampler.next2D());
        sample.dist = Ray::infinity();
        sample.pdf = INV_FOUR_PI;
        return true;
    } else {
        Vec2f uv = _emission->sample(MAP_SPHERICAL, sampler.next2D());
        float sinTheta;
        sample.d = uvToDirection(uv, sinTheta);
        sample.pdf = INV_PI*INV_TWO_PI*_emission->pdf(MAP_SPHERICAL, uv)/sinTheta;
        sample.dist = Ray::infinity();
        return sample.pdf != 0.0f;
    }
}

bool InfiniteSphere::invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const
{
    float faceXi;
    Vec2f xi;
    if (!SampleWarp::invertProjectedBox(_sceneBounds, point.p, -point.Ng, faceXi, xi, sampler.untracked1D()))
        return false;

    sampler.put1D(faceXi);
    sampler.put2D(xi);

    return true;
}

bool InfiniteSphere::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    if (_emission->isConstant())
        sampler.put2D(SampleWarp::invertUniformSphere(-direction.d, sampler.untracked1D()));
    else
        sampler.put2D(_emission->invert(MAP_SPHERICAL, directionToUV(-direction.d)));

    return true;
}

float InfiniteSphere::positionalPdf(const PositionSample &point) const
{
    return SampleWarp::projectedBoxPdf(_sceneBounds, point.Ng);
}

float InfiniteSphere::directionalPdf(const PositionSample &point, const DirectionSample &/*sample*/) const
{
    if (_emission->isConstant()) {
        return INV_FOUR_PI;
    } else {
        float sinTheta;
        directionToUV(-point.Ng, sinTheta);
        return INV_PI*INV_TWO_PI*_emission->pdf(MAP_SPHERICAL, point.uv)/sinTheta;
    }
}

float InfiniteSphere::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &data,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/) const
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

Vec3f InfiniteSphere::evalPositionalEmission(const PositionSample &/*sample*/) const
{
    return Vec3f(1.0f);
}

Vec3f InfiniteSphere::evalDirectionalEmission(const PositionSample &point, const DirectionSample &/*sample*/) const
{
    return (*_emission)[point.uv];
}

Vec3f InfiniteSphere::evalDirect(const IntersectionTemporary &/*data*/, const IntersectionInfo &info) const
{
    return (*_emission)[info.uv];
}

bool InfiniteSphere::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool InfiniteSphere::isDirac() const
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

    Primitive::prepareForRender();
}

int InfiniteSphere::numBsdfs() const
{
    return 0;
}

std::shared_ptr<Bsdf> &InfiniteSphere::bsdf(int /*index*/)
{
    FAIL("InfiniteSphere::bsdf should not be called");
}

void InfiniteSphere::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &/*bsdf*/)
{
}

Primitive *InfiniteSphere::clone()
{
    return new InfiniteSphere(*this);
}

}
