#include "Disk.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

struct DiskIntersection
{
    Vec3f p;
    float rSq;
    bool backSide;
};

Disk::Disk()
: _coneAngle(90.0f),
  _bsdf(_defaultBsdf)
{
}

Disk::Disk(const Vec3f &pos, const Vec3f &n, float r, const std::string &name, std::shared_ptr<Bsdf> bsdf)
: Primitive(name),
  _coneAngle(90.0f),
  _center(pos),
  _r(r),
  _frame(n),
  _bsdf(std::move(bsdf))
{
    _transform = Mat4f::translate(_center)*Mat4f(_frame.tangent, _frame.normal, -_frame.bitangent)*Mat4f::scale(Vec3f(r));
}

void Disk::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cone", false, false);
    _proxy->makeCone(1.0f, 0.01f);
}

float Disk::powerToRadianceFactor() const
{
    return INV_PI*_invArea;
}

void Disk::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);
    value.getField("cone_angle", _coneAngle);

    if (auto bsdf = value["bsdf"])
        _bsdf = scene.fetchBsdf(bsdf);
}

rapidjson::Value Disk::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "disk",
        "cone_angle", _coneAngle,
        "bsdf", *_bsdf
    };
}

bool Disk::intersect(Ray &ray, IntersectionTemporary &data) const
{
    float nDotW = ray.dir().dot(_n);

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

bool Disk::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    Vec2f xi = sampler.next2D();
    Vec2f lQ = SampleWarp::uniformDisk(xi).xy()*_r;
    sample.p = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
    sample.pdf = _invArea;
    sample.uv = Vec2f(xi.x() + 0.5f, std::sqrt(xi.y()));
    if (sample.uv.x() > 1.0f)
        sample.uv.x() -= 1.0f;
    sample.weight = PI*_area*(*_emission)[sample.uv];
    sample.Ng = _n;

    return true;
}

bool Disk::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, DirectionSample &sample) const
{
    // TODO: Cone angle
    Vec3f d = SampleWarp::cosineHemisphere(sampler.next2D());
    sample.d = _frame.toGlobal(d);
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::cosineHemispherePdf(d);

    return true;
}

bool Disk::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const
{
    if (_n.dot(p - _center) < 0.0f)
        return false;

    Vec2f lQ = SampleWarp::uniformDisk(sampler.next2D()).xy()*_r;
    Vec3f q = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
    sample.d = q - p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    if (-sample.d.dot(_n) < _cosApex)
        return false;
    float cosTheta = -_n.dot(sample.d);
    sample.pdf = rSq/(cosTheta*_r*_r*PI);
    return true;
}

bool Disk::invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const
{
    Vec3f p = point.p - _center;
    Vec3f lQ = Vec3f(_frame.bitangent.dot(p)/_r, _frame.tangent.dot(p)/_r, 0.0f);
    sampler.put2D(SampleWarp::invertUniformDisk(lQ, sampler.untracked1D()));
    return true;
}

bool Disk::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    Vec3f localD = _frame.toLocal(direction.d);
    if (localD.z() <= 0.0f)
        return false;

    sampler.put2D(SampleWarp::invertCosineHemisphere(localD, sampler.untracked1D()));
    return true;
}

float Disk::positionalPdf(const PositionSample &/*point*/) const
{
    return _invArea;
}

float Disk::directionalPdf(const PositionSample &/*point*/, const DirectionSample &sample) const
{
    // TODO: Cone angle
    return max(sample.d.dot(_frame.normal)*INV_PI, 0.0f);
}

float Disk::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &info, const Vec3f &p) const
{
    float cosTheta = std::abs(_n.dot(info.w));
    float t = _n.dot(_center - p)/_n.dot(info.w);

    return t*t/(cosTheta*_r*_r*PI);
}

Vec3f Disk::evalPositionalEmission(const PositionSample &sample) const
{
    return PI*(*_emission)[sample.uv];
}

Vec3f Disk::evalDirectionalEmission(const PositionSample &/*point*/, const DirectionSample &sample) const
{
    // TODO: Cone angle
    return Vec3f(max(sample.d.dot(_frame.normal), 0.0f)*INV_PI);
}

Vec3f Disk::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    return data.as<DiskIntersection>()->backSide ? Vec3f(0.0f) : (*_emission)[info.uv];
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
    _area = _r*_r*PI;
    _invArea = 1.0f/_area;
    _frame = TangentFrame(_n);
    _cosApex = std::cos(Angle::degToRad(_coneAngle));
    _coneBase = _center - _n/std::sin(Angle::degToRad(_coneAngle));

    Primitive::prepareForRender();
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
