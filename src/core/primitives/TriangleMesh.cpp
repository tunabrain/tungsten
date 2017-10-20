#include "TriangleMesh.hpp"
#include "EmbreeUtil.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/TangentFrame.hpp"
#include "math/Mat4f.hpp"
#include "math/Vec.hpp"
#include "math/Box.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonObject.hpp"
#include "io/MeshIO.hpp"
#include "io/Scene.hpp"

#include <unordered_map>
#include <iostream>

namespace Tungsten {

struct MeshIntersection
{
    Vec3f Ng;
    float u;
    float v;
    int primId;
    bool backSide;
};

TriangleMesh::TriangleMesh()
: _smoothed(false),
  _backfaceCulling(false),
  _recomputeNormals(false),
  _bsdfs(1, _defaultBsdf),
  _scene(nullptr)
{
}

TriangleMesh::TriangleMesh(const TriangleMesh &o)
: Primitive(o),
  _path(o._path),
  _smoothed(o._smoothed),
  _backfaceCulling(o._backfaceCulling),
  _recomputeNormals(o._recomputeNormals),
  _verts(o._verts),
  _tris(o._tris),
  _bsdfs(o._bsdfs),
  _bounds(o._bounds)
{
}

TriangleMesh::TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
             const std::shared_ptr<Bsdf> &bsdf,
             const std::string &name, bool smoothed, bool backfaceCull)
: TriangleMesh(
      std::move(verts),
      std::move(tris),
      std::vector<std::shared_ptr<Bsdf>>(1, bsdf),
      name,
      smoothed,
      backfaceCull
  )
{
}

TriangleMesh::TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
             std::vector<std::shared_ptr<Bsdf>> bsdfs,
             const std::string &name, bool smoothed, bool backfaceCull)
: Primitive(name),
  _path(std::make_shared<Path>(std::string(name).append(".wo3"))),
  _smoothed(smoothed),
  _backfaceCulling(backfaceCull),
  _recomputeNormals(false),
  _verts(std::move(verts)),
  _tris(std::move(tris)),
  _bsdfs(std::move(bsdfs))
{
}

Vec3f TriangleMesh::unnormalizedGeometricNormalAt(int triangle) const
{
    const TriangleI &t = _tris[triangle];
    Vec3f p0 = _tfVerts[t.v0].pos();
    Vec3f p1 = _tfVerts[t.v1].pos();
    Vec3f p2 = _tfVerts[t.v2].pos();
    return (p1 - p0).cross(p2 - p0);
}

Vec3f TriangleMesh::normalAt(int triangle, float u, float v) const
{
    const TriangleI &t = _tris[triangle];
    Vec3f n0 = _tfVerts[t.v0].normal();
    Vec3f n1 = _tfVerts[t.v1].normal();
    Vec3f n2 = _tfVerts[t.v2].normal();
    return ((1.0f - u - v)*n0 + u*n1 + v*n2).normalized();
}

Vec2f TriangleMesh::uvAt(int triangle, float u, float v) const
{
    const TriangleI &t = _tris[triangle];
    Vec2f uv0 = _tfVerts[t.v0].uv();
    Vec2f uv1 = _tfVerts[t.v1].uv();
    Vec2f uv2 = _tfVerts[t.v2].uv();
    return (1.0f - u - v)*uv0 + u*uv1 + v*uv2;
}

float TriangleMesh::powerToRadianceFactor() const
{
    return INV_PI*_invArea;
}


void TriangleMesh::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);

    if (auto path = value["file"]) _path = scene.fetchResource(path);
    value.getField("smooth", _smoothed);
    value.getField("backface_culling", _backfaceCulling);
    value.getField("recompute_normals", _recomputeNormals);

    if (auto bsdf = value["bsdf"]) {
        _bsdfs.clear();
        if (bsdf.isArray())
            for (unsigned i = 0; i < bsdf.size(); ++i)
                _bsdfs.emplace_back(scene.fetchBsdf(bsdf[i]));
        else
            _bsdfs.emplace_back(scene.fetchBsdf(bsdf));
    }
}

rapidjson::Value TriangleMesh::toJson(Allocator &allocator) const
{
    JsonObject result{Primitive::toJson(allocator), allocator,
        "type", "mesh",
        "smooth", _smoothed,
        "backface_culling", _backfaceCulling,
        "recompute_normals", _recomputeNormals
    };
    if (_path)
        result.add("file", *_path);
    if (_bsdfs.size() == 1) {
        result.add("bsdf", *_bsdfs[0]);
    } else {
        rapidjson::Value a(rapidjson::kArrayType);
        for (const auto &bsdf : _bsdfs)
            a.PushBack(bsdf->toJson(allocator), allocator);
        result.add("bsdf", std::move(a));
    }

    return result;
}

void TriangleMesh::loadResources()
{
    if (_path && !MeshIO::load(*_path, _verts, _tris))
        DBG("Unable to load triangle mesh at %s", *_path);
    if (_recomputeNormals && _smoothed)
        calcSmoothVertexNormals();
}

void TriangleMesh::saveResources()
{
    if (_path)
        saveAs(*_path);
}

void TriangleMesh::saveAs(const Path &path) const
{
    MeshIO::save(path, _verts, _tris);
}

void TriangleMesh::calcSmoothVertexNormals()
{
    static const float SplitLimit = std::cos(PI*0.15f);
    //static CONSTEXPR float SplitLimit = -1.0f;

    std::vector<Vec3f> geometricN(_verts.size(), Vec3f(0.0f));
    std::unordered_multimap<Vec3f, uint32> posToVert;

    for (uint32 i = 0; i < _verts.size(); ++i) {
        _verts[i].normal() = Vec3f(0.0f);
        posToVert.insert(std::make_pair(_verts[i].pos(), i));
    }

    for (TriangleI &t : _tris) {
        const Vec3f &p0 = _verts[t.v0].pos();
        const Vec3f &p1 = _verts[t.v1].pos();
        const Vec3f &p2 = _verts[t.v2].pos();
        Vec3f normal = (p1 - p0).cross(p2 - p0);
        if (normal == 0.0f)
            normal = Vec3f(0.0f, 1.0f, 0.0f);
        else
            normal.normalize();

        for (int i = 0; i < 3; ++i) {
            Vec3f &n = geometricN[t.vs[i]];
            if (n == 0.0f) {
                n = normal;
            } else if (n.dot(normal) < SplitLimit) {
                _verts.push_back(_verts[t.vs[i]]);
                geometricN.push_back(normal);
                t.vs[i] = _verts.size() - 1;
            }
        }
    }

    for (TriangleI &t : _tris) {
        const Vec3f &p0 = _verts[t.v0].pos();
        const Vec3f &p1 = _verts[t.v1].pos();
        const Vec3f &p2 = _verts[t.v2].pos();
        Vec3f normal = (p1 - p0).cross(p2 - p0);
        Vec3f nN = normal.normalized();

        for (int i = 0; i < 3; ++i) {
            auto iters = posToVert.equal_range(_verts[t.vs[i]].pos());

            for (auto t = iters.first; t != iters.second; ++t)
                if (geometricN[t->second].dot(nN) >= SplitLimit)
                    _verts[t->second].normal() += normal;
        }
    }

    for (uint32 i = 0; i < _verts.size(); ++i) {
        if (_verts[i].normal() == 0.0f)
            _verts[i].normal() = geometricN[i];
        else
            _verts[i].normal().normalize();
    }
}

void TriangleMesh::computeBounds()
{
    Box3f box;
    for (Vertex &v : _verts)
        box.grow(_transform*v.pos());
    _bounds = box;
}

void TriangleMesh::makeCube()
{
    const Vec3f verts[6][4] = {
        {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f, -0.5f}},
        {{-0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}},
        {{-0.5f,  0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}},
        {{-0.5f,  0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}},
    };
    const Vec2f uvs[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

    for (int i = 0; i < 6; ++i) {
        int idx = _verts.size();
        _tris.emplace_back(idx, idx + 2, idx + 1);
        _tris.emplace_back(idx, idx + 3, idx + 2);

        for (int j = 0; j < 4; ++j)
            _verts.emplace_back(verts[i][j], uvs[j]);
    }
}

void TriangleMesh::makeSphere(float radius)
{
    CONSTEXPR int SubDiv = 10;
    CONSTEXPR int Skip = SubDiv*2 + 1;
    for (int f = 0, idx = _verts.size(); f < 3; ++f) {
        for (int s = -1; s <= 1; s += 2) {
            for (int u = -SubDiv; u <= SubDiv; ++u) {
                for (int v = -SubDiv; v <= SubDiv; ++v, ++idx) {
                    Vec3f p(0.0f);
                    p[f] = s;
                    p[(f + 1) % 3] = u*(1.0f/SubDiv)*s;
                    p[(f + 2) % 3] = v*(1.0f/SubDiv);
                    _verts.emplace_back(p.normalized()*radius);

                    if (v > -SubDiv && u > -SubDiv) {
                        _tris.emplace_back(idx - Skip - 1, idx, idx - Skip);
                        _tris.emplace_back(idx - Skip - 1, idx - 1, idx);
                    }
                }
            }
        }
    }
}

void TriangleMesh::makeCone(float radius, float height)
{
    CONSTEXPR int SubDiv = 36;
    int base = _verts.size();
    _verts.emplace_back(Vec3f(0.0f));
    for (int i = 0; i < SubDiv; ++i) {
        float a = i*TWO_PI/SubDiv;
        _verts.emplace_back(Vec3f(std::cos(a)*radius, height, std::sin(a)*radius));
        _tris.emplace_back(base, base + i + 1, base + ((i + 1) % SubDiv) + 1);
    }
}

void TriangleMesh::makeCylinder(float radius, float height)
{
    CONSTEXPR int SubDiv = 36;
    int base = _verts.size();
    _verts.emplace_back(Vec3f(0.0f, -height, 0.0f));
    _verts.emplace_back(Vec3f(0.0f,  height, 0.0f));
    for (int i = 0; i < SubDiv; ++i) {
        float a = i*TWO_PI/SubDiv;
        _verts.emplace_back(Vec3f(std::cos(a)*radius, -height, std::sin(a)*radius));
        _verts.emplace_back(Vec3f(std::cos(a)*radius,  height, std::sin(a)*radius));
        int i1 = (i + 1) % SubDiv;
        _tris.emplace_back(base + 0, base + 2 + i*2, base + 2 + i1*2);
        _tris.emplace_back(base + 1, base + 3 + i*2, base + 3 + i1*2);
        _tris.emplace_back(base + 2 + i *2, base + 3 + i*2, base + 2 + i1*2);
        _tris.emplace_back(base + 2 + i1*2, base + 3 + i*2, base + 3 + i1*2);
    }
}

bool TriangleMesh::intersect(Ray &ray, IntersectionTemporary &data) const
{
    RTCRay eRay(EmbreeUtil::convert(ray));
    rtcIntersect(_scene, eRay);
    if (eRay.geomID != RTC_INVALID_GEOMETRY_ID) {
        ray.setFarT(eRay.tfar);

        data.primitive = this;
        MeshIntersection *isect = data.as<MeshIntersection>();
        isect->Ng = unnormalizedGeometricNormalAt(eRay.primID);
        isect->u = eRay.u;
        isect->v = eRay.v;
        isect->primId = eRay.primID;
        isect->backSide = isect->Ng.dot(ray.dir()) > 0.0f;

        return true;
    }
    return false;
}

bool TriangleMesh::occluded(const Ray &ray) const
{
    RTCRay eRay(EmbreeUtil::convert(ray));
    rtcOccluded(_scene, eRay);
    return eRay.geomID != RTC_INVALID_GEOMETRY_ID;
}

void TriangleMesh::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const MeshIntersection *isect = data.as<MeshIntersection>();
    info.Ng = isect->Ng.normalized();
    if (_smoothed)
        info.Ns = normalAt(isect->primId, isect->u, isect->v);
    else
        info.Ns = info.Ng;
    info.uv = uvAt(isect->primId, isect->u, isect->v);
    info.primitive = this;
    info.bsdf = _bsdfs[_tris[isect->primId].material].get();
}

bool TriangleMesh::hitBackside(const IntersectionTemporary &data) const
{
    return data.as<MeshIntersection>()->backSide;
}

bool TriangleMesh::tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &/*info*/,
        Vec3f &T, Vec3f &B) const
{
    const MeshIntersection *isect = data.as<MeshIntersection>();
    const TriangleI &t = _tris[isect->primId];
    Vec3f p0 = _tfVerts[t.v0].pos();
    Vec3f p1 = _tfVerts[t.v1].pos();
    Vec3f p2 = _tfVerts[t.v2].pos();
    Vec2f uv0 = _tfVerts[t.v0].uv();
    Vec2f uv1 = _tfVerts[t.v1].uv();
    Vec2f uv2 = _tfVerts[t.v2].uv();
    Vec3f q1 = p1 - p0;
    Vec3f q2 = p2 - p0;
    float s1 = uv1.x() - uv0.x(), t1 = uv1.y() - uv0.y();
    float s2 = uv2.x() - uv0.x(), t2 = uv2.y() - uv0.y();
    float invDet = s1*t2 - s2*t1;
    if (std::abs(invDet) < 1e-6f)
        return false;
    T = (q1*t2 - t1*q2).normalized();
    B = (q2*s1 - s2*q1).normalized();

    return true;
}

const TriangleMesh &TriangleMesh::asTriangleMesh()
{
    return *this;
}

bool TriangleMesh::isSamplable() const
{
    return true;
}

void TriangleMesh::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
    if (_triSampler)
        return;

    std::vector<float> areas(_tris.size());
    _totalArea = 0.0f;
    for (size_t i = 0; i < _tris.size(); ++i) {
        Vec3f p0 = _tfVerts[_tris[i].v0].pos();
        Vec3f p1 = _tfVerts[_tris[i].v1].pos();
        Vec3f p2 = _tfVerts[_tris[i].v2].pos();
        areas[i] = MathUtil::triangleArea(p0, p1, p2);
        _totalArea += areas[i];
    }
    _triSampler.reset(new Distribution1D(std::move(areas)));
}

bool TriangleMesh::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    float u = sampler.next1D();
    int idx;
    _triSampler->warp(u, idx);

    Vec3f p0 = _tfVerts[_tris[idx].v0].pos();
    Vec3f p1 = _tfVerts[_tris[idx].v1].pos();
    Vec3f p2 = _tfVerts[_tris[idx].v2].pos();
    Vec2f uv0 = _tfVerts[_tris[idx].v0].uv();
    Vec2f uv1 = _tfVerts[_tris[idx].v1].uv();
    Vec2f uv2 = _tfVerts[_tris[idx].v2].uv();
    Vec3f normal = (p1 - p0).cross(p2 - p0).normalized();

    Vec2f lambda = SampleWarp::uniformTriangleUv(sampler.next2D());

    sample.p = p0*lambda.x() + p1*lambda.y() + p2*(1.0f - lambda.x() - lambda.y());
    sample.uv = uv0*lambda.x() + uv1*lambda.y() + uv2*(1.0f - lambda.x() - lambda.y());
    sample.weight = PI*_totalArea*(*_emission)[sample.uv];
    sample.pdf = _invArea;
    sample.Ng = normal;

    return true;
}

bool TriangleMesh::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const
{
    Vec3f d = SampleWarp::cosineHemisphere(sampler.next2D());
    sample.d = TangentFrame(point.Ng).toGlobal(d);
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::cosineHemispherePdf(d);

    return true;
}

bool TriangleMesh::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p,
        PathSampleGenerator &sampler, LightSample &sample) const
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
    sample.pdf = rSq/(cosTheta*_totalArea);

    return true;
}

float TriangleMesh::positionalPdf(const PositionSample &/*point*/) const
{
    return _invArea;
}

float TriangleMesh::directionalPdf(const PositionSample &point, const DirectionSample &sample) const
{
    return max(sample.d.dot(point.Ng)*INV_PI, 0.0f);
}

float TriangleMesh::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &info, const Vec3f &p) const
{
    return (p - info.p).lengthSq()/(-info.w.dot(info.Ng)*_totalArea);
}

Vec3f TriangleMesh::evalPositionalEmission(const PositionSample &sample) const
{
    return PI*(*_emission)[sample.uv];
}

Vec3f TriangleMesh::evalDirectionalEmission(const PositionSample &point, const DirectionSample &sample) const
{
    return Vec3f(max(sample.d.dot(point.Ng), 0.0f)*INV_PI);
}

Vec3f TriangleMesh::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    return data.as<MeshIntersection>()->backSide ? Vec3f(0.0f) : (*_emission)[info.uv];
}

bool TriangleMesh::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool TriangleMesh::isDirac() const
{
    return _verts.empty() || _tris.empty();
}

bool TriangleMesh::isInfinite() const
{
    return false;
}

// Questionable, but there is no cheap and realiable way to compute this factor
float TriangleMesh::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return -1.0f;
}

Box3f TriangleMesh::bounds() const
{
    return _bounds;
}

void TriangleMesh::prepareForRender()
{
    computeBounds();

    if (_verts.empty() || _tris.empty())
        return;

    _scene = rtcDeviceNewScene(EmbreeUtil::getDevice(), RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT, RTC_INTERSECT1);
    _geomId = rtcNewTriangleMesh(_scene, RTC_GEOMETRY_STATIC, _tris.size(), _verts.size(), 1);
    Vec4f *vs = static_cast<Vec4f *>(rtcMapBuffer(_scene, _geomId, RTC_VERTEX_BUFFER));
    Vec3u *ts = static_cast<Vec3u *>(rtcMapBuffer(_scene, _geomId, RTC_INDEX_BUFFER));

    for (size_t i = 0; i < _tris.size(); ++i) {
        const TriangleI &t = _tris[i];
        _tris[i].material = clamp(_tris[i].material, 0, int(_bsdfs.size()) - 1);
        ts[i] = Vec3u(t.v0, t.v1, t.v2);
    }

    _tfVerts.resize(_verts.size());
    Mat4f normalTform(_transform.toNormalMatrix());
    for (size_t i = 0; i < _verts.size(); ++i) {
        _tfVerts[i] = Vertex(
            _transform*_verts[i].pos(),
            normalTform.transformVector(_verts[i].normal()),
            _verts[i].uv()
        );
        const Vec3f &p = _tfVerts[i].pos();
        vs[i] = Vec4f(p.x(), p.y(), p.z(), 0.0f);
    }

    _totalArea = 0.0f;
    for (size_t i = 0; i < _tris.size(); ++i) {
        Vec3f p0 = _tfVerts[_tris[i].v0].pos();
        Vec3f p1 = _tfVerts[_tris[i].v1].pos();
        Vec3f p2 = _tfVerts[_tris[i].v2].pos();
        _totalArea += MathUtil::triangleArea(p0, p1, p2);
    }
    _invArea = 1.0f/_totalArea;

    rtcUnmapBuffer(_scene, _geomId, RTC_VERTEX_BUFFER);
    rtcUnmapBuffer(_scene, _geomId, RTC_INDEX_BUFFER);

    rtcCommit(_scene);

    //if (_backfaceCulling)
    // TODO

    Primitive::prepareForRender();
}

void TriangleMesh::teardownAfterRender()
{
    if (_scene)  {
        rtcDeleteGeometry(_scene, _geomId);
        rtcDeleteScene(_scene);
        _scene = nullptr;
    }
    _tfVerts.clear();

    Primitive::teardownAfterRender();
}

int TriangleMesh::numBsdfs() const
{
    return _bsdfs.size();
}

std::shared_ptr<Bsdf> &TriangleMesh::bsdf(int index)
{
    return _bsdfs[index];
}

void TriangleMesh::setBsdf(int index, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdfs[index] = bsdf;
}

Primitive *TriangleMesh::clone()
{
    return new TriangleMesh(*this);
}

}
