#include "Cylinder.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

struct CylinderIntersection
{
    Vec3f n;
    Vec2f uv;
    bool hitCap;
    bool backSide;
};

Cylinder::Cylinder()
: _capped(true),
  _bsdf(_defaultBsdf)
{
}

void Cylinder::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cylinder", false, false);
    _proxy->makeCylinder(0.5f, 0.5f);
}

float Cylinder::powerToRadianceFactor() const
{
    return INV_PI*_invArea;
}

void Cylinder::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);

    value.getField("capped", _capped);

    if (auto bsdf = value["bsdf"])
        _bsdf = scene.fetchBsdf(bsdf);
}

rapidjson::Value Cylinder::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "cylinder",
        "capped", _capped,
        "bsdf", *_bsdf
    };
}

bool Cylinder::intersect(Ray &ray, IntersectionTemporary &data) const
{
    Vec3f pLocal = _invRot*(ray.pos() - _pos);
    Vec3f dLocal = _invRot*ray.dir();
    Vec2f p = pLocal.xz()*_invRadius;
    Vec2f d = dLocal.xz()*_invRadius;

    bool didHit = false;
    CylinderIntersection *isect = data.as<CylinderIntersection>();

    if (_capped && std::abs(dLocal.y()) > 1e-6f) {
        for (float sign : {1.0f, -1.0f}) {
            float t = (sign*_halfHeight - pLocal.y())/dLocal.y();
            if (t > ray.nearT() && t < ray.farT()) {
                Vec2f pHit = p + t*d;
                if (pHit.lengthSq() < 1.0f) {
                    didHit = true;
                    isect->n = Vec3f(0.0f, sign, 0.0f);
                    isect->uv = pHit*0.5f + 0.5f;
                    isect->hitCap = true;
                    isect->backSide = sign*dLocal.y() > 0.0f;
                    ray.setFarT(t);
                }
            }
        }
    }

    float A = d.dot(d);
    float B = p.dot(d);
    float C = p.lengthSq() - 1.0f;
    float detSq = B*B - A*C;
    if (detSq >= 0.0f) {
        float det = std::sqrt(detSq);
        for (float sign : {1.0f, -1.0f}) {
            float t = (-B - sign*det)/A;
            if (t > ray.nearT() && t < ray.farT()) {
                float h = pLocal.y() + dLocal.y()*t;
                if (h >= -_halfHeight && h <= _halfHeight) {
                    didHit = true;
                    Vec2f pHit = p + t*d;
                    isect->n = Vec3f(pHit.x(), 0.0f, pHit.y());
                    isect->uv = Vec2f(0.0f, h*_invHeight + 0.5f);
                    isect->hitCap = false;
                    isect->backSide = sign < 0.0f;
                    ray.setFarT(t);
                }
            }
        }
    }
    if (didHit)
        data.primitive = this;

    return didHit;
}

bool Cylinder::occluded(const Ray &ray) const
{
    IntersectionTemporary data;
    Ray ray_ = ray;
    return intersect(ray_, data);
}

bool Cylinder::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<CylinderIntersection>()->backSide;
}

void Cylinder::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const CylinderIntersection *isect = data.as<CylinderIntersection>();
    info.Ng = info.Ns = _rot*isect->n;
    if (isect->hitCap)
        info.uv = isect->uv;
    else
        info.uv = Vec2f(std::atan2(isect->n.z(), isect->n.x())*INV_TWO_PI + 0.5f, isect->uv.y());
    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool Cylinder::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &info,
        Vec3f &T, Vec3f &B) const
{
    T = _axis;
    B = info.Ng.cross(T);
    return true;
}

bool Cylinder::isSamplable() const
{
    return true;
}

void Cylinder::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Cylinder::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    if (_capped && sampler.nextBoolean(TWO_PI*sqr(_radius)*_invArea)) {
        Vec3f pd = SampleWarp::uniformDisk(sampler.next2D());
        float sign = sampler.nextBoolean(0.5f) ? -1.0f : 1.0f;
        sample.Ng = Vec3f(0.0f, sign, 0.0f);
        sample.p = Vec3f(pd.x()*_radius, sign*_halfHeight, pd.y()*_radius);
        sample.uv = pd.xy() + 0.5f;
    } else {
        Vec2f xi = sampler.next2D();
        Vec3f pc = SampleWarp::uniformCylinder(xi);
        sample.Ng = Vec3f(pc.x(), 0.0f, pc.y());
        sample.p = Vec3f(pc.x()*_radius, pc.z()*_halfHeight, pc.y()*_radius);
        sample.uv = xi;
    }
    sample.Ng = _rot*sample.Ng;
    sample.p = _rot*sample.p + _pos;
    sample.pdf = _invArea;
    sample.weight = PI*_area*(*_emission)[sample.uv];

    return true;
}

bool Cylinder::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const
{
    Vec3f d = SampleWarp::cosineHemisphere(sampler.next2D());
    sample.d = TangentFrame(point.Ng).toGlobal(d);
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::cosineHemispherePdf(d);

    return true;
}

bool Cylinder::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const
{
    PositionSample point;
    samplePosition(sampler, point);

    Vec3f L = point.p - p;

    float rSq = L.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d = L/sample.dist;
    float cosTheta = -(point.Ng.dot(sample.d));
    if (cosTheta <= 0.0f)
        return false;
    sample.pdf = rSq/(cosTheta*_area);

    return true;
}

bool Cylinder::invertPosition(WritablePathSampleGenerator &/*sampler*/, const PositionSample &/*point*/) const
{
    // TODO
    return false;
}

bool Cylinder::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &point,
        const DirectionSample &direction) const
{
    Vec3f localD = TangentFrame(point.Ng).toLocal(direction.d);
    if (localD.z() <= 0.0f)
        return false;

    sampler.put2D(SampleWarp::invertCosineHemisphere(localD, sampler.untracked1D()));
    return true;
}

float Cylinder::positionalPdf(const PositionSample &/*point*/) const
{
    return _invArea;
}

float Cylinder::directionalPdf(const PositionSample &point, const DirectionSample &sample) const
{
    return max(sample.d.dot(point.Ng)*INV_PI, 0.0f);
}

float Cylinder::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &info, const Vec3f &p) const
{
    return (p - info.p).lengthSq()/(-info.w.dot(info.Ng)*_area);
}

Vec3f Cylinder::evalPositionalEmission(const PositionSample &sample) const
{
    return PI*(*_emission)[sample.uv];
}

Vec3f Cylinder::evalDirectionalEmission(const PositionSample &point, const DirectionSample &sample) const
{
    return Vec3f(max(sample.d.dot(point.Ng), 0.0f)*INV_PI);
}

Vec3f Cylinder::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    return data.as<CylinderIntersection>()->backSide ? Vec3f(0.0f) : (*_emission)[info.uv];
}

bool Cylinder::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Cylinder::isDirac() const
{
    return false;
}

bool Cylinder::isInfinite() const
{
    return false;
}

float Cylinder::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    // TODO
    return -1.0f;
}

Box3f Cylinder::bounds() const
{
    Box3f result;
    result.grow(_pos + _axis*_halfHeight);
    result.grow(_pos - _axis*_halfHeight);
    result.grow(_radius);
    return result;
}

const TriangleMesh &Cylinder::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Cylinder::prepareForRender()
{
    _rot = _transform.extractRotation();
    _invRot = _rot.transpose();
    _pos = _transform*Vec3f(0.0f);
    _axis = _transform.up().normalized();
    Vec3f scale = _transform.extractScaleVec();
    _radius = 0.5f*scale.xz().max();
    _invRadius = 1.0f/_radius;
    _halfHeight = 0.5f*scale.y();
    _invHeight = 0.5f/_halfHeight;
    _area = 2.0f*PI*sqr(_radius) + TWO_PI*_radius*2.0f*_halfHeight;
    _invArea = 1.0f/_area;

    Primitive::prepareForRender();
}

int Cylinder::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Cylinder::bsdf(int /*index*/)
{
    return _bsdf;
}

void Cylinder::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *Cylinder::clone()
{
    return new Cylinder(*this);
}

}
