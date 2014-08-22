#include "Quad.hpp"
#include "Mesh.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

namespace Tungsten {

struct QuadIntersection
{
    Vec3f p;
    float u;
    float v;
    bool backSide;
};

Quad::Quad(const Vec3f &base, const Vec3f &edge0, const Vec3f &edge1,
        const std::string &name, std::shared_ptr<Bsdf> bsdf)
: Primitive(name, bsdf)
{
    _transform = Mat4f::translate(base + edge0*0.5f + edge1*0.5f)*Mat4f(edge0, edge0.cross(edge1), edge1);
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

    _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "QuadLight", false));
}

void Quad::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
}

rapidjson::Value Quad::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "quad", allocator);
    return std::move(v);
}

bool Quad::intersect(Ray &ray, IntersectionTemporary &data) const
{
    float nDotW = ray.dir().dot(_n);

    float t = _n.dot(_base - ray.pos())/nDotW;
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
    isect->v = 1.0f - l1;
    isect->backSide = nDotW >= 0.0f;
    data.primitive = this;

    return true;
}

bool Quad::occluded(const Ray &ray) const
{
    float nDotW = ray.dir().dot(_n);

    float t = _n.dot(_base - ray.pos())/nDotW;
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
    info.Ng = info.Ns = _n;
    info.p = isect->p;
    info.uv = Vec2f(isect->u, isect->v);
    info.primitive = this;
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

void Quad::makeSamplable()
{
}

float Quad::inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &p, const Vec3f &d) const
{
    float cosTheta = std::abs(_n.dot(d));
    float t = _n.dot(_base - p)/_n.dot(d);

    return t*t/(cosTheta*_area);
}

bool Quad::sampleInboundDirection(LightSample &sample) const
{
    if (_n.dot(sample.p - _base) < 0.0f)
        return false;

    Vec2f xi = sample.sampler->next2D();
    Vec3f q = _base + xi.x()*_edge0 + xi.y()*_edge1;
    sample.d = q - sample.p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    float cosTheta = -_n.dot(sample.d);
    sample.pdf = rSq/(cosTheta*_area);
    return true;
}

bool Quad::sampleOutboundDirection(LightSample &sample) const
{
    Vec2f xi = sample.sampler->next2D();
    sample.p = _base + xi.x()*_edge0 + xi.y()*_edge1;
    sample.d = SampleWarp::cosineHemisphere(sample.sampler->next2D());
    sample.pdf = SampleWarp::cosineHemispherePdf(sample.d)/_area;
    TangentFrame frame(_n);
    sample.d = frame.toGlobal(sample.d);
    return true;
}

bool Quad::invertParametrization(Vec2f uv, Vec3f &pos) const
{
    pos = _base + uv.x()*_edge0 + uv.y()*_edge1;
    return true;
}

bool Quad::isDelta() const
{
    return false;
}

bool Quad::isInfinite() const
{
    return false;
}

float Quad::approximateRadiance(const Vec3f &p) const
{
    if (!isEmissive())
        return 0.0f;
    Vec3f R0 = _base - p;

    if (R0.dot(_n) >= 0.0f)
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

    _n = _edge0.cross(_edge1);
    _area = _n.length();
    _n /= _area;

    _invUvSq = 1.0f/Vec2f(_edge0.lengthSq(), _edge1.lengthSq());
}

void Quad::cleanupAfterRender()
{
}

Primitive *Quad::clone()
{
    return new Quad(*this);
}

}
