#include "Cube.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

struct CubeIntersection
{
    bool backSide;
};

Cube::Cube()
: _pos(0.0f),
  _scale(0.5f)
{
}

Cube::Cube(const Vec3f &pos, const Vec3f &scale, const Mat4f &rot,
        const std::string &name, std::shared_ptr<Bsdf> bsdf)
: Primitive(name),
  _rot(rot),
  _invRot(rot.transpose()),
  _pos(pos),
  _scale(scale*0.5f),
  _bsdf(std::move(bsdf))
{
    _transform = Mat4f::translate(_pos)*rot*Mat4f::scale(Vec3f(scale));
}

void Cube::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cube", false, false);
    _proxy->makeCube();
}

inline int Cube::sampleFace(float &u) const
{
    u *= _faceCdf.z();
    if (u < _faceCdf.x()) {
        u /= _faceCdf.x();
        return 0;
    } else if (u < _faceCdf.y()) {
        u = (u - _faceCdf.x())/(_faceCdf.y() - _faceCdf.x());
        return 1;
    } else {
        u = (u - _faceCdf.y())/(_faceCdf.z() - _faceCdf.y());
        return 2;
    }
}

float Cube::powerToRadianceFactor() const
{
    return INV_PI*_invArea;
}

void Cube::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);

    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));
}

rapidjson::Value Cube::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "cube", allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    return std::move(v);
}

bool Cube::intersect(Ray &ray, IntersectionTemporary &data) const
{
    Vec3f p = _invRot*(ray.pos() - _pos);
    Vec3f d = _invRot*ray.dir();

    Vec3f invD = 1.0f/d;
    Vec3f relMin((-_scale - p));
    Vec3f relMax(( _scale - p));

    float ttMin = ray.nearT(), ttMax = ray.farT();
    for (int i = 0; i < 3; ++i) {
        if (invD[i] >= 0.0f) {
            ttMin = max(ttMin, relMin[i]*invD[i]);
            ttMax = min(ttMax, relMax[i]*invD[i]);
        } else {
            ttMax = min(ttMax, relMin[i]*invD[i]);
            ttMin = max(ttMin, relMax[i]*invD[i]);
        }
    }

    if (ttMin <= ttMax) {
        data.primitive = this;
        if (ttMin == ray.nearT()) {
            ray.setFarT(ttMax);
            data.as<CubeIntersection>()->backSide = true;
        } else {
            ray.setFarT(ttMin);
            data.as<CubeIntersection>()->backSide = false;
        }
        return true;
    }
    return false;
}

bool Cube::occluded(const Ray &ray) const
{
    Vec3f p = _invRot*(ray.pos() - _pos);
    Vec3f d = _invRot*ray.dir();

    Vec3f invD = 1.0f/d;
    Vec3f relMin((-_scale - p));
    Vec3f relMax(( _scale - p));

    float ttMin = ray.nearT(), ttMax = ray.farT();
    for (int i = 0; i < 3; ++i) {
        if (invD[i] >= 0.0f) {
            ttMin = max(ttMin, relMin[i]*invD[i]);
            ttMax = min(ttMax, relMax[i]*invD[i]);
        } else {
            ttMax = min(ttMax, relMin[i]*invD[i]);
            ttMin = max(ttMin, relMax[i]*invD[i]);
        }
    }

    return ttMin <= ttMax;
}

bool Cube::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<CubeIntersection>()->backSide;
}

void Cube::intersectionInfo(const IntersectionTemporary &/*data*/, IntersectionInfo &info) const
{
    Vec3f p = _invRot*(info.p - _pos);
    Vec3f n(0.0f);
    int dim = (std::abs(p) - _scale).maxDim();
    n[dim] = p[dim] < 0.0f ? -1.0f : 1.0f;

    Vec3f uvw = (p/_scale)*0.5f + 0.5f;

    info.Ns = info.Ng = _rot*n;
    info.uv = Vec2f(uvw[(dim + 1) % 3], uvw[(dim + 2) % 3]);
    info.primitive = this;
    info.bsdf = _bsdf.get();
}

bool Cube::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &info, Vec3f &T, Vec3f &B) const
{
    Vec3f p = _invRot*(info.p - _pos);
    int dim = (std::abs(p) - _scale).maxDim();
    T = B = Vec3f(0.0f);
    T[(dim + 1) % 3] = 1.0f;
    B[(dim + 2) % 3] = 1.0f;
    T = _rot*T;
    B = _rot*B;
    return true;
}

bool Cube::isSamplable() const
{
    return true;
}

void Cube::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Cube::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    float u = sampler.next1D(EmitterSample);
    int dim = sampleFace(u);
    float s = (dim + 1) % 3;
    float t = (dim + 2) % 3;

    Vec2f xi = sampler.next2D(EmitterSample);

    Vec3f n(0.0f);
    n[dim] = u < 0.5f ? -1.0f : 1.0f;

    Vec3f p(0.0f);
    p[dim] = n[dim]*_scale[dim];
    p[s] = (xi.x()*2.0f - 1.0f)*_scale[s];
    p[t] = (xi.y()*2.0f - 1.0f)*_scale[t];

    sample.p = _rot*p + _pos;
    sample.pdf = _invArea;
    sample.uv = xi;
    sample.weight = PI*_area*(*_emission)[sample.uv];
    sample.Ng = _rot*n;

    return true;
}

bool Cube::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const
{
    Vec3f d = SampleWarp::cosineHemisphere(sampler.next2D(EmitterSample));
    sample.d = TangentFrame(point.Ng).toGlobal(d);
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::cosineHemispherePdf(d);

    return true;
}

bool Cube::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const
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

float Cube::positionalPdf(const PositionSample &/*point*/) const
{
    return _invArea;
}

float Cube::directionalPdf(const PositionSample &point, const DirectionSample &sample) const
{
    return max(sample.d.dot(point.Ng)*INV_PI, 0.0f);
}

float Cube::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &info, const Vec3f &p) const
{
    return (p - info.p).lengthSq()/(-info.w.dot(info.Ng)*_area);
}

Vec3f Cube::evalPositionalEmission(const PositionSample &sample) const
{
    return PI*(*_emission)[sample.uv];
}

Vec3f Cube::evalDirectionalEmission(const PositionSample &point, const DirectionSample &sample) const
{
    return Vec3f(max(sample.d.dot(point.Ng), 0.0f)*INV_PI);
}

Vec3f Cube::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    return data.as<CubeIntersection>()->backSide ? Vec3f(0.0f) : (*_emission)[info.uv];
}

bool Cube::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Cube::isDirac() const
{
    return false;
}

bool Cube::isInfinite() const
{
    return false;
}

float Cube::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    float dSq = max(std::abs(_invRot*(p - _pos)), Vec3f(0.0f)).lengthSq();
    return _emission->average().max()*_faceCdf.z()/dSq;
}

Box3f Cube::bounds() const
{
    Box3f box;
    for (int i = 0; i < 8; ++i) {
        box.grow(_pos + _rot*Vec3f(
            (i & 1 ? _scale.x() : -_scale.x()),
            (i & 2 ? _scale.y() : -_scale.y()),
            (i & 4 ? _scale.z() : -_scale.z())
        ));
    }
    return box;
}

const TriangleMesh &Cube::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Cube::prepareForRender()
{
    _pos = _transform*Vec3f(0.0f);
    _scale = _transform.extractScale()*Vec3f(0.5f);
    _rot = _transform.extractRotation();
    _invRot = _rot.transpose();
    _faceCdf = 4.0f*Vec3f(
        _scale.y()*_scale.z(),
        _scale.z()*_scale.x(),
        _scale.x()*_scale.y()
    );
    _faceCdf.y() += _faceCdf.x();
    _faceCdf.z() += _faceCdf.y();
    _area = 2.0f*_faceCdf.z();
    _invArea = 1.0f/_area;

    Primitive::prepareForRender();
}

int Cube::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Cube::bsdf(int /*index*/)
{
    return _bsdf;
}

void Cube::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *Cube::clone()
{
    return new Cube(*this);
}

}
