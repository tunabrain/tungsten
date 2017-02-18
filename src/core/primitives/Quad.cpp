#include "Quad.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

struct QuadIntersection
{
    Vec3f p;
    float u;
    float v;
    bool backSide;
};

Quad::Quad()
: _bsdf(_defaultBsdf)
{
}

Quad::Quad(const Vec3f &base, const Vec3f &edge0, const Vec3f &edge1,
        const std::string &name, std::shared_ptr<Bsdf> bsdf)
: Primitive(name),
  _bsdf(std::move(bsdf))
{

    _transform = Mat4f::translate(base + edge0*0.5f + edge1*0.5f)*Mat4f(edge0, edge1.cross(edge0), edge1);
}

void Quad::buildProxy()
{
    std::vector<Vertex> verts{{
        Vec3f(-0.5f, 0.0f, -0.5f),
        Vec3f( 0.5f, 0.0f, -0.5f),
        Vec3f( 0.5f, 0.0f,  0.5f),
        Vec3f(-0.5f, 0.0f,  0.5f),
    }};
    std::vector<TriangleI> tris{{
        TriangleI{0, 1, 2},
        TriangleI{0, 2, 3}
    }};

    _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "QuadLight", false, false));
}

float Quad::powerToRadianceFactor() const
{
    return INV_PI*_invArea;
}

void Quad::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);

    if (auto bsdf = value["bsdf"])
        _bsdf = scene.fetchBsdf(bsdf);
}

rapidjson::Value Quad::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "quad",
        "bsdf", *_bsdf
    };
}

bool Quad::intersect(Ray &ray, IntersectionTemporary &data) const
{
    float nDotW = ray.dir().dot(_frame.normal);
    if (std::abs(nDotW) < 1e-6f)
        return false;

    float t = _frame.normal.dot(_base - ray.pos())/nDotW;
    if (t < ray.nearT() || t > ray.farT())
        return false;

    Vec3f q = ray.pos() + t*ray.dir();
    Vec3f v = q - _base;
    float l0 = v.dot(_edge0)*_invUvSq.x();
    float l1 = v.dot(_edge1)*_invUvSq.y();

    if (l0 < 0.0f || l0 > 1.0f || l1 < 0.0f || l1 > 1.0f)
        return false;

    ray.setFarT(t);
    QuadIntersection *isect = data.as<QuadIntersection>();
    isect->p = q;
    isect->u = l0;
    isect->v = l1;
    isect->backSide = nDotW >= 0.0f;
    data.primitive = this;

    return true;
}

bool Quad::occluded(const Ray &ray) const
{
    float nDotW = ray.dir().dot(_frame.normal);

    float t = _frame.normal.dot(_base - ray.pos())/nDotW;
    if (t < ray.nearT() || t > ray.farT())
        return false;

    Vec3f q = ray.pos() + t*ray.dir();
    Vec3f v = q - _base;
    float l0 = v.dot(_edge0)*_invUvSq.x();
    float l1 = v.dot(_edge1)*_invUvSq.y();

    if (l0 < 0.0f || l0 > 1.0f || l1 < 0.0f || l1 > 1.0f)
        return false;
    return true;
}

bool Quad::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<QuadIntersection>()->backSide;
}

void Quad::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const QuadIntersection *isect = data.as<QuadIntersection>();
    info.Ng = info.Ns = _frame.normal;
    info.p = isect->p;
    info.uv = Vec2f(isect->u, isect->v);
    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool Quad::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &T, Vec3f &B) const
{
    T = _edge0;
    B = _edge1;
    return true;
}

bool Quad::isSamplable() const
{
    return true;
}

void Quad::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Quad::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    Vec2f xi = sampler.next2D();
    sample.p = _base + xi.x()*_edge0 + xi.y()*_edge1;
    sample.pdf = _invArea;
    sample.uv = xi;
    sample.weight = PI*_area*(*_emission)[sample.uv];
    sample.Ng = _frame.normal;

    return true;
}

bool Quad::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, DirectionSample &sample) const
{
    Vec3f d = SampleWarp::cosineHemisphere(sampler.next2D());
    sample.d = _frame.toGlobal(d);
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::cosineHemispherePdf(d);

    return true;
}

bool Quad::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const
{
    if (_frame.normal.dot(p - _base) <= 0.0f)
        return false;

    Vec2f xi = sampler.next2D();
    Vec3f q = _base + xi.x()*_edge0 + xi.y()*_edge1;
    sample.d = q - p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    float cosTheta = -_frame.normal.dot(sample.d);
    sample.pdf = rSq/(cosTheta*_area);

    return true;
}

bool Quad::invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const
{
    sampler.put2D(point.uv);
    return true;
}

bool Quad::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    Vec3f localD = _frame.toLocal(direction.d);
    if (localD.z() <= 0.0f)
        return false;

    sampler.put2D(SampleWarp::invertCosineHemisphere(localD, sampler.untracked1D()));
    return true;
}

float Quad::positionalPdf(const PositionSample &/*point*/) const
{
    return _invArea;
}

float Quad::directionalPdf(const PositionSample &/*point*/, const DirectionSample &sample) const
{
    return max(sample.d.dot(_frame.normal)*INV_PI, 0.0f);
}

float Quad::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &info, const Vec3f &p) const
{
    float cosTheta = std::abs(_frame.normal.dot(info.w));
    float t = _frame.normal.dot(_base - p)/_frame.normal.dot(info.w);

    return t*t/(cosTheta*_area);
}

Vec3f Quad::evalPositionalEmission(const PositionSample &sample) const
{
    return PI*(*_emission)[sample.uv];
}

Vec3f Quad::evalDirectionalEmission(const PositionSample &/*point*/, const DirectionSample &sample) const
{
    return Vec3f(max(sample.d.dot(_frame.normal), 0.0f)*INV_PI);
}

Vec3f Quad::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    return data.as<QuadIntersection>()->backSide ? Vec3f(0.0f) : (*_emission)[info.uv];
}

bool Quad::invertParametrization(Vec2f uv, Vec3f &pos) const
{
    pos = _base + uv.x()*_edge0 + uv.y()*_edge1;
    return true;
}

bool Quad::isDirac() const
{
    return false;
}

bool Quad::isInfinite() const
{
    return false;
}

float Quad::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    if (!isEmissive())
        return 0.0f;
    Vec3f R0 = _base - p;

    if (R0.dot(_frame.normal) >= 0.0f)
        return 0.0f;

    Vec3f R1 = R0 + _edge0;
    Vec3f R2 = R1 + _edge1;
    Vec3f R3 = R0 + _edge1;
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

Box3f Quad::bounds() const
{
    Box3f result;
    result.grow(_base);
    result.grow(_base + _edge0);
    result.grow(_base + _edge1);
    result.grow(_base + _edge0 + _edge1);
    return result;
}

const TriangleMesh &Quad::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Quad::prepareForRender()
{
    _base = _transform*Vec3f(0.0f);
    _edge0 = _transform.transformVector(Vec3f(1.0f, 0.0f, 0.0f));
    _edge1 = _transform.transformVector(Vec3f(0.0f, 0.0f, 1.0f));
    _base -= _edge0*0.5f;
    _base -= _edge1*0.5f;

    Vec3f n = _edge1.cross(_edge0);
    _area = n.length();
    _invArea = 1.0f/_area;
    n /= _area;

    _frame = TangentFrame(n, _edge0.normalized(), _edge1.normalized());

    _invUvSq = 1.0f/Vec2f(_edge0.lengthSq(), _edge1.lengthSq());

    Primitive::prepareForRender();
}

int Quad::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Quad::bsdf(int /*index*/)
{
    return _bsdf;
}

void Quad::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *Quad::clone()
{
    return new Quad(*this);
}

}
