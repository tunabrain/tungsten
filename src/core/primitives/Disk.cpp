#include "Disk.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

struct DiskIntersection
{
    Vec3f p;
    float rSq;
    bool backSide;
};

Disk::Disk()
: _coneAngle(45.0f)
{
}

void Disk::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cone", false, false);
    _proxy->makeCone(1.0f, 0.01f);
}

void Disk::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
    JsonUtils::fromJson(v, "cone_angle", _coneAngle);

    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));
}

rapidjson::Value Disk::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "disk", allocator);
    v.AddMember("cone_angle", _coneAngle, allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
}

bool Disk::intersect(Ray &ray, IntersectionTemporary &data) const
{
    float nDotW = ray.dir().dot(_n);
    if (nDotW >= 0.0f)
        return false;

    float t = _n.dot(_center - ray.pos())/nDotW;
    if (t < ray.nearT() || t > ray.farT())
        return false;

    Vec3f q = ray.pos() + t*ray.dir();
    Vec3f v = q - _center;
    float rSq = v.lengthSq();
    if (rSq > _r*_r)
        return false;

    ray.setFarT(t);
    DiskIntersection *isect = data.as<DiskIntersection>();
    isect->p = q;
    isect->rSq = rSq;
    isect->backSide = -nDotW < _cosApex;
    data.primitive = this;

    return true;
}

bool Disk::occluded(const Ray &ray) const
{
    float nDotW = ray.dir().dot(_n);
    if (nDotW >= 0.0f)
        return false;

    float t = _n.dot(_center - ray.pos())/nDotW;
    if (t < ray.nearT() || t > ray.farT())
        return false;

    Vec3f q = ray.pos() + t*ray.dir();
    Vec3f v = q - _center;
    float rSq = v.lengthSq();
    if (rSq > _r*_r)
        return false;

    return true;
}

bool Disk::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<DiskIntersection>()->backSide;
}

void Disk::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const DiskIntersection *isect = data.as<DiskIntersection>();
    info.Ng = info.Ns = _n;

    info.p = isect->p;
    Vec3f d = isect->p - _center;
    float x = d.dot(_frame.bitangent);
    float y = d.dot(_frame.tangent);
    float v = std::sqrt(isect->rSq)/_r;
    float u = (x == 0.0f && y == 0.0f) ? 0.0f : (std::atan2(y, x)*INV_TWO_PI + 0.5f);
    info.uv = Vec2f(u, v);

    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool Disk::tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &/*info*/, Vec3f &T, Vec3f &B) const
{
    const DiskIntersection *isect = data.as<DiskIntersection>();
    Vec3f d = isect->p - _center;
    if (d.lengthSq() == 0.0f)
        return false;
    d.normalize();

    T = _n.cross(d);
    B = d;
    return true;
}

bool Disk::isSamplable() const
{
    return true;
}

void Disk::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

float Disk::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &p, const Vec3f &d) const
{
    float cosTheta = std::abs(_n.dot(d));
    float t = _n.dot(_center - p)/_n.dot(d);

    return t*t/(cosTheta*_r*_r*PI);
}

bool Disk::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    if (_n.dot(sample.p - _center) < 0.0f)
        return false;

    Vec2f lQ = SampleWarp::uniformDisk(sample.sampler->next2D(EmitterSample)).xy()*_r;
    Vec3f q = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
    sample.d = q - sample.p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    if (-sample.d.dot(_n) < _cosApex)
        return false;
    float cosTheta = -_n.dot(sample.d);
    sample.pdf = rSq/(cosTheta*_r*_r*PI);
    return true;
}

bool Disk::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    Vec2f xi = sample.sampler->next2D(EmitterSample);
    Vec2f lQ = SampleWarp::uniformDisk(xi).xy();
    sample.p = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
    sample.d = SampleWarp::cosineHemisphere(sample.sampler->next2D(EmitterSample));
    sample.pdf = SampleWarp::cosineHemispherePdf(sample.d)/(_r*_r*PI);
    TangentFrame frame(_n);
    sample.weight = PI*(*_emission)[xi]*(_r*_r*PI);
    sample.d = frame.toGlobal(sample.d);
    sample.medium = _bsdf->overridesMedia() ? _bsdf->extMedium().get() : nullptr;
    return true;
}

bool Disk::invertParametrization(Vec2f uv, Vec3f &pos) const
{
    float phi = (uv.x() - 0.5f)*TWO_PI;
    float r = uv.y()*_r;
    pos = _center + std::cos(phi)*r*_frame.bitangent + std::sin(phi)*r*_frame.tangent;
    return true;
}

bool Disk::isDirac() const
{
    return false;
}

bool Disk::isInfinite() const
{
    return false;
}

float Disk::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    if (!isEmissive())
        return 0.0f;
    Vec3f coneD = p - _coneBase;
    if (coneD.dot(_n)/coneD.length() < _cosApex)
        return 0.0f;

    Vec3f d = _center - p;

    Vec3f edge0 = _frame.tangent*_r;
    Vec3f edge1 = _frame.bitangent*_r;

    Vec3f R0 = d - edge0 - edge1;
    Vec3f R1 = R0 + 2.0f*edge0;
    Vec3f R2 = R1 + 2.0f*edge1;
    Vec3f R3 = R0 + 2.0f*edge1;
    Vec3f n0 = R0.cross(R1).normalized();
    Vec3f n1 = R1.cross(R2).normalized();
    Vec3f n2 = R2.cross(R3).normalized();
    Vec3f n3 = R3.cross(R0).normalized();
    float Q =
          std::acos(n0.dot(n1))
        + std::acos(n1.dot(n2))
        + std::acos(n2.dot(n3))
        + std::acos(n3.dot(n0));

    return (TWO_PI - std::abs(Q))*_emission->average().max();
}

Box3f Disk::bounds() const
{
    Box3f result;
    result.grow(_center - _frame.tangent*_r - _frame.bitangent*_r);
    result.grow(_center + _frame.tangent*_r - _frame.bitangent*_r);
    result.grow(_center + _frame.tangent*_r + _frame.bitangent*_r);
    result.grow(_center - _frame.tangent*_r + _frame.bitangent*_r);
    return result;
}

const TriangleMesh &Disk::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Disk::prepareForRender()
{
    _center = _transform*Vec3f(0.0f);
    _r = (_transform.extractScale()*Vec3f(1.0f, 0.0f, 1.0f)).max();
    _n = _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();
    _frame = TangentFrame(_n);
    _cosApex = std::cos(Angle::degToRad(_coneAngle));
    _coneBase = _center - _n/std::sin(Angle::degToRad(_coneAngle));
}

void Disk::teardownAfterRender()
{
}

int Disk::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Disk::bsdf(int /*index*/)
{
    return _bsdf;
}

void Disk::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *Disk::clone()
{
    return new Disk(*this);
}

}
