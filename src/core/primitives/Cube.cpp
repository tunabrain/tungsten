#include "Cube.hpp"
#include "TriangleMesh.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

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
: Primitive(name, std::move(bsdf)),
  _rot(rot),
  _invRot(rot.transpose()),
  _pos(pos),
  _scale(scale*0.5f)
{
    _transform = Mat4f::translate(_pos)*rot*Mat4f::scale(Vec3f(scale));
}

void Cube::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cube", false);
    _proxy->makeCube();
}

void Cube::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
}

rapidjson::Value Cube::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "cube", allocator);
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
    return false;
}

void Cube::makeSamplable()
{
}

float Cube::inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    return 0.0f;
}

bool Cube::sampleInboundDirection(LightSample &/*sample*/) const
{
    return false;
}

bool Cube::sampleOutboundDirection(LightSample &/*sample*/) const
{
    return false;
}

bool Cube::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Cube::isDelta() const
{
    return false;
}

bool Cube::isInfinite() const
{
    return false;
}

float Cube::approximateRadiance(const Vec3f &/*p*/) const
{
    return 0.0f;
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
}

void Cube::cleanupAfterRender()
{
}

Primitive *Cube::clone()
{
    return new Cube(*this);
}

}
