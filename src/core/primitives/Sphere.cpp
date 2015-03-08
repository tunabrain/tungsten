#include "Sphere.hpp"
#include "TriangleMesh.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

struct SphereIntersection
{
    bool backSide;
};

Sphere::Sphere()
: _pos(0.0f),
  _radius(1.0f)
{
}

Sphere::Sphere(const Vec3f &pos, float r, const std::string &name, std::shared_ptr<Bsdf> bsdf)
: Primitive(name),
  _pos(pos),
  _radius(r),
  _bsdf(std::move(bsdf))
{
    _transform = Mat4f::translate(_pos)*Mat4f::scale(Vec3f(_radius));
}

float Sphere::solidAngle(const Vec3f &p) const
{
    Vec3f L = _pos - p;
    float d = L.length();
    float cosTheta = std::sqrt(max(d*d - _radius*_radius, 0.0f))/d;

    return TWO_PI*(1.0f - cosTheta);
}

void Sphere::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Sphere", false, false);
    _proxy->makeSphere(1.0f);
}

void Sphere::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);

    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));
}

rapidjson::Value Sphere::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "sphere", allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
}

bool Sphere::intersect(Ray &ray, IntersectionTemporary &data) const
{
    Vec3f p = ray.pos() - _pos;
    float B = p.dot(ray.dir());
    float C = p.lengthSq() - _radius*_radius;
    float detSq = B*B - C;
    if (detSq >= 0.0f) {
        float det = std::sqrt(detSq);
        float t = -B - det;
        if (t < ray.farT() && t > ray.nearT()) {
            ray.setFarT(t);
            data.primitive = this;
            data.as<SphereIntersection>()->backSide = false;
            return true;
        }
        t = -B + det;
        if (t < ray.farT() && t > ray.nearT()) {
            ray.setFarT(t);
            data.primitive = this;
            data.as<SphereIntersection>()->backSide = true;
            return true;
        }
    }

    return false;
}

bool Sphere::occluded(const Ray &ray) const
{
    Vec3f p = ray.pos() - _pos;
    float B = p.dot(ray.dir());
    float C = p.lengthSq() - _radius*_radius;
    float detSq = B*B - C;
    if (detSq >= 0.0f) {
        float det = std::sqrt(detSq);
        float t = -B - det;
        if (t < ray.farT() && t > ray.nearT())
            return true;
        t = -B + det;
        if (t < ray.farT() && t > ray.nearT())
            return true;
    }

    return false;
}

bool Sphere::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<SphereIntersection>()->backSide;
}

void Sphere::intersectionInfo(const IntersectionTemporary &/*data*/, IntersectionInfo &info) const
{
    info.Ns = info.Ng = (info.p - _pos)/_radius;
    Vec3f localN = _invRot.transformVector(info.Ng);
    info.uv = Vec2f(std::atan2(localN.y(), localN.x())*INV_TWO_PI + 0.5f, std::acos(clamp(localN.z(), -1.0f, 1.0f))*INV_PI);
    if (std::isnan(info.uv.x()))
        info.uv.x() = 0.0f;
    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool Sphere::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &info, Vec3f &T, Vec3f &B) const
{
    Vec3f localN = _invRot.transformVector(info.Ng);
    T = _rot.transformVector(Vec3f(-localN.y(), localN.x(), localN.z()));
    B = info.Ns.cross(T);
    return true;
}

bool Sphere::isSamplable() const
{
    return true;
}

void Sphere::makeSamplable(uint32 /*threadIndex*/)
{
}

float Sphere::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &p, const Vec3f &/*d*/) const
{
    float dist = (_pos - p).length();
    float cosTheta = std::sqrt(max(dist*dist - _radius*_radius, 0.0f))/dist;
    return SampleWarp::uniformSphericalCapPdf(cosTheta);
}

bool Sphere::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    Vec3f L = _pos - sample.p;
    float d = L.length();
    L.normalize();
    float cosTheta = std::sqrt(max(d*d - _radius*_radius, 0.0f))/d;
    sample.d = SampleWarp::uniformSphericalCap(sample.sampler->next2D(), cosTheta);

    TangentFrame frame(L);
    sample.dist = d;
    sample.d = frame.toGlobal(sample.d);
    sample.pdf = SampleWarp::uniformSphericalCapPdf(cosTheta);

    return true;
}

bool Sphere::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    sample.p = SampleWarp::uniformSphere(sample.sampler->next2D());
    sample.d = SampleWarp::cosineHemisphere(sample.sampler->next2D());
    sample.pdf = SampleWarp::cosineHemispherePdf(sample.d)/(FOUR_PI*_radius*_radius);
    TangentFrame frame(sample.p);
    sample.d = frame.toGlobal(sample.d);
    sample.p = sample.p*_radius + _pos;

    return true;
}

bool Sphere::invertParametrization(Vec2f uv, Vec3f &pos) const
{
    float phi = uv.x()*TWO_PI - PI;
    float theta = uv.y()*PI;
    Vec3f localPos = Vec3f(std::cos(phi)*std::sin(theta), std::sin(phi)*std::sin(theta), std::cos(theta));
    pos = _rot.transformVector(localPos*_radius) + _pos;
    return true;
}

bool Sphere::isDelta() const
{
    return false;
}

bool Sphere::isInfinite() const
{
    return false;
}

float Sphere::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    if (!isEmissive())
        return 0.0f;
    return solidAngle(p)*_emission->average().max();
}

Box3f Sphere::bounds() const
{
    return Box3f(_pos - _radius, _pos + _radius);
}

const TriangleMesh &Sphere::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Sphere::prepareForRender()
{
    _pos = _transform*Vec3f(0.0f);
    _radius = (_transform.extractScale()*Vec3f(1.0f)).max();
    _rot = _transform.extractRotation();
    _invRot = _rot.transpose();
}

void Sphere::teardownAfterRender()
{
}

int Sphere::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Sphere::bsdf(int /*index*/)
{
    return _bsdf;
}

Primitive *Sphere::clone()
{
    return new Sphere(*this);
}

}
