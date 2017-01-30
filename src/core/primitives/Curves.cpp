#include "Curves.hpp"
#include "TriangleMesh.hpp"

#include "sampling/UniformSampler.hpp"

#include "bsdfs/HairBcsdf.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/BSpline.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"
#include "io/FileUtils.hpp"
#include "io/CurveIO.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(Curves::CurveMode, "curve mode", ({
    {"cylinder", Curves::MODE_CYLINDER},
    {"half_cylinder", Curves::MODE_HALF_CYLINDER},
    {"bcsdf_cylinder", Curves::MODE_BCSDF_CYLINDER},
    {"ribbon", Curves::MODE_RIBBON}
}))

struct CurveIntersection
{
    uint32 curveP0;
    float t;
    Vec2f uv;
    float w;
};

struct StackNode
{
    Vec4f p0, p1;
    float tMin, tMax;
    int depth;

    void set(float tMin_, float tMax_, Vec4f p0_, Vec4f p1_, int depth_)
    {
        p0 = p0_;
        p1 = p1_;
        tMin = tMin_;
        tMax = tMax_;
        depth = depth_;
    }
};

static inline void intersectHalfCylinder(StackNode node, float tMin,
        float &tMax, CurveIntersection &isect)
{
    Vec2f v = (node.p1.xy() - node.p0.xy());
    float lengthSq = v.lengthSq();
    float invLengthSq = 1.0f/lengthSq;
    float invLength = std::sqrt(invLengthSq);
    float segmentT = -(node.p0.xy().dot(v))*invLengthSq;
    float signedUnnormalized = node.p0.x()*v.y() - node.p0.y()*v.x();
    float distance = std::fabs(signedUnnormalized)*invLength;

    float width = node.p0.w()*(1.0f - segmentT) + node.p1.w()*segmentT;
    if (distance > width)
        return;

    float depth = node.p0.z()*(1.0f - segmentT) + node.p1.z()*segmentT;
    float dz = node.p1.z() - node.p0.z();
    float ySq = sqr(width) - sqr(distance);
    float lSq = ySq*(1.0f + dz*dz*invLengthSq);
    float deltaT = std::sqrt(std::max(lSq, 0.0f));
    float t0 = depth - deltaT;

    Vec3f v3(node.p0.xyz() - node.p1.xyz());
    lengthSq = v3.lengthSq();
    segmentT = (Vec3f(node.p0.x(), node.p0.y(), node.p0.z() - t0)).dot(v3)/lengthSq;
    if (segmentT < 0.0f || t0 >= tMax || t0 <= tMin) {
        // Enable this for two-sided cylinders (for transmissive BSDFs)
        // Not really recommended (self-intersections), so disabled for now

//        t0 = depth + deltaT;
//        segmentT -= 2.0f*deltaT*v3.z()/lengthSq;
//        if (segmentT < 0.0f || t0 >= closestDepth || t0 <= tMin)
            return;
    }

    float newT = segmentT*(node.tMax - node.tMin) + node.tMin;

    if (newT >= 0.0f && newT <= 1.0f) {
        isect.uv = Vec2f(newT, 0.5f + 0.5f*distance/width);
        isect.t = t0;
        isect.w = width;
        tMax = t0;
    }
}

static inline void intersectRibbon(StackNode node, float tMin, float &tMax,
        CurveIntersection &isect, Vec3f n0, Vec3f n1, Vec3f n2)
{
    Vec3f v = (node.p1.xyz() - node.p0.xyz());
    float lengthSq = v.lengthSq();
    if (lengthSq == 0.0f)
        return;
    float invLengthSq = 1.0f/lengthSq;

    float tMid = (node.tMin + node.tMax)*0.5f;
    Vec3f n = BSpline::quadratic(n0, n1, n2, tMid);

    n = (v*(v.dot(n)*invLengthSq) - n);

    float t0 = n.dot(node.p0.xyz())/n.z();
    if (t0 < tMin || t0 > tMax)
        return;

    Vec3f localP = Vec3f(-node.p0.x(), -node.p0.y(), t0 - node.p0.z());
    float dot = localP.dot(v);
    float segmentT = dot*invLengthSq;
    if (segmentT < 0.0f || segmentT > 1.0f)
        return;

    float width = node.p0.w()*(1.0f - segmentT) + node.p1.w()*segmentT;
    float distSq = (localP - v*segmentT).lengthSq();

    if (distSq > width*width)
        return;

    float newT = segmentT*(node.tMax - node.tMin) + node.tMin;
    if (newT >= 0.0f && newT <= 1.0f) {
        isect.uv = Vec2f(newT, 0.0f);
        isect.t = t0;
        isect.w = width;
        tMax = t0;
    }
}

template<typename T>
static inline void precomputeBSplineCoefficients(T &p0, T &p1, T &p2)
{
    T q0 = (0.5f*p0 - p1 + 0.5f*p2);
    T q1 = (p1 - p0);
    T q2 = 0.5f*(p0 + p1);
    p0 = q0;
    p1 = q1;
    p2 = q2;
}

// Implementation of "Ray tracing for curves primitive" by Nakamaru and Ohno
// http://wscg.zcu.cz/wscg2002/Papers_2002/A83.pdf
template<bool isRibbon>
static bool pointOnSpline(Vec4f q0, Vec4f q1, Vec4f q2,
        float tMin, float tMax, CurveIntersection &isect,
        Vec3f n0, Vec3f n1, Vec3f n2)
{

    CONSTEXPR int MaxDepth = 5;

    StackNode stackBuf[MaxDepth];
    StackNode *stack = &stackBuf[0];

    precomputeBSplineCoefficients(q0, q1, q2);

    Vec2f tFlat = -q1.xy()*0.5f/q0.xy();
    Vec2f xyFlat = q0.xy()*tFlat*tFlat + q1.xy()*tFlat + q2.xy();
    float xFlat = xyFlat.x(), yFlat = xyFlat.y();

    StackNode cur{
        q2,
        q0 + q1 + q2,
        0.0f, 1.0f, 0
    };
    float closestDepth = tMax;

    while (true) {
        Vec2f pMin = min(cur.p0.xy(), cur.p1.xy());
        Vec2f pMax = max(cur.p0.xy(), cur.p1.xy());
        if (tFlat.x() > cur.tMin && tFlat.x() < cur.tMax) {
            pMin.x() = min(pMin.x(), xFlat);
            pMax.x() = max(pMax.x(), xFlat);
        }
        if (tFlat.y() > cur.tMin && tFlat.y() < cur.tMax) {
            pMin.y() = min(pMin.y(), yFlat);
            pMax.y() = max(pMax.y(), yFlat);
        }

        float maxWidth = max(cur.p0.w(), cur.p1.w());
        if (pMin.x() <= maxWidth && pMin.y() <= maxWidth && pMax.x() >= -maxWidth && pMax.y() >= -maxWidth) {
            if (cur.depth >= MaxDepth) {
                if (isRibbon)
                    intersectRibbon(cur, tMin, closestDepth, isect, n0, n1, n2);
                else
                    intersectHalfCylinder(cur, tMin, closestDepth, isect);
            } else {
                float splitT = (cur.tMin + cur.tMax)*0.5f;
                Vec4f qSplit = q0*(splitT*splitT) + q1*splitT + q2;

                if (cur.p0.z() < qSplit.z()) {
                    (*stack++).set(splitT, cur.tMax, qSplit, cur.p1, cur.depth + 1);
                    cur.set(cur.tMin, splitT, cur.p0, qSplit, cur.depth + 1);
                } else {
                    (*stack++).set(cur.tMin, splitT, cur.p0, qSplit, cur.depth + 1);
                    cur.set(splitT, cur.tMax, qSplit, cur.p1, cur.depth + 1);
                }
                continue;
            }
        }
        do {
            if (stack == stackBuf)
                return closestDepth < tMax;
            cur = *--stack;
        } while (min(cur.p0.z() - cur.p0.w(), cur.p1.z() - cur.p1.w()) > closestDepth);
    }

    return false;
}

static Vec4f project(const Vec3f &o, const Vec3f &lx, const Vec3f &ly, const Vec3f &lz, const Vec4f &q)
{
    Vec3f p(q.xyz() - o);
    return Vec4f(lx.dot(p), ly.dot(p), lz.dot(p), q.w());
}

static Vec3f project(const Vec3f &lx, const Vec3f &ly, const Vec3f &lz, const Vec3f &q)
{
    return Vec3f(lx.dot(q), ly.dot(q), lz.dot(q));
}

static Box3f curveBox(const Vec4f &q0, const Vec4f &q1, const Vec4f &q2)
{
    Vec2f xMinMax(BSpline::quadraticMinMax(q0.x(), q1.x(), q2.x()));
    Vec2f yMinMax(BSpline::quadraticMinMax(q0.y(), q1.y(), q2.y()));
    Vec2f zMinMax(BSpline::quadraticMinMax(q0.z(), q1.z(), q2.z()));
    float maxW = max(q0.w(), q1.w(), q2.w());
    return Box3f(
        Vec3f(xMinMax.x(), yMinMax.x(), zMinMax.x()) - maxW,
        Vec3f(xMinMax.y(), yMinMax.y(), zMinMax.y()) + maxW
    );
}

Curves::Curves()
: _mode("half_cylinder"),
  _curveThickness(0.01f),
  _subsample(0.0f),
  _overrideThickness(false),
  _taperThickness(false),
  _bsdf(std::make_shared<HairBcsdf>())
{
}

Curves::Curves(const Curves &o)
: Primitive(o)
{
    _mode              = o._mode;
    _curveThickness    = o._curveThickness;
    _taperThickness    = o._taperThickness;
    _overrideThickness = o._overrideThickness;
    _path              = o._path;
    _curveCount        = o._curveCount;
    _nodeCount         = o._nodeCount;
    _curveEnds         = o._curveEnds;
    _nodeData          = o._nodeData;
    _nodeColor         = o._nodeColor;
    _nodeNormals       = o._nodeNormals;
    _bsdf              = o._bsdf;
    _proxy             = o._proxy;
    _bounds            = o._bounds;
}

Curves::Curves(std::vector<uint32> curveEnds, std::vector<Vec4f> nodeData, std::shared_ptr<Bsdf> bsdf, std::string name)
: Primitive(name),
  _path(std::make_shared<Path>(name.append(".fiber"))),
  _mode("half_cylinder"),
  _curveThickness(0.01f),
  _overrideThickness(false),
  _taperThickness(false),
  _curveCount(curveEnds.size()),
  _nodeCount(nodeData.size()),
  _curveEnds(std::move(curveEnds)),
  _nodeData(std::move(nodeData)),
  _bsdf(std::move(bsdf))
{
}

void Curves::loadCurves()
{
    CurveIO::CurveData data;
    data.curveEnds = &_curveEnds;
    data.nodeData  = &_nodeData;
    data.nodeColor = &_nodeColor;
    data.nodeNormal = &_nodeNormals;

    if (_path && !CurveIO::load(*_path, data))
        DBG("Unable to load curves at %s", *_path);

    _nodeCount = _nodeData.size();
    _curveCount = _curveEnds.size();

    if (_overrideThickness || _taperThickness) {
        for (uint32 i = 0; i < _curveCount; ++i) {
            uint32 start = i ? _curveEnds[i - 1] : 0;
            for (uint32 t = start; t < _curveEnds[i]; ++t) {
                float thickness = _overrideThickness ? _curveThickness : _nodeData[t].w();
                if (_taperThickness)
                    thickness *= 1.0f - (t - start - 0.5f)/(_curveEnds[i] - start - 1);
                _nodeData[t].w() = thickness;
            }
        }
    }
}

void Curves::computeBounds()
{
    _bounds = Box3f();
    for (size_t i = 2; i < _nodeData.size(); ++i)
        _bounds.grow(curveBox(_nodeData[i - 2], _nodeData[i - 1], _nodeData[i]));
}

void Curves::buildProxy()
{
    std::vector<Vertex> verts;
    std::vector<TriangleI> tris;

    int segmentCount = 0;
    for (uint32 i = 0; i < _curveCount; ++i)
        segmentCount += _curveEnds[i] - (i ? _curveEnds[i - 1] : 0) - 1;

    const int MaxSegments = 150000;

    int samples, stepSize;
    if (segmentCount < MaxSegments) {
        stepSize = 1;
        samples = min(MaxSegments/segmentCount, 10);
    } else {
        stepSize = segmentCount/MaxSegments;
        samples = 1;
    }

    uint32 idx = 0;
    for (uint32 i = 0; i < _curveCount; i += stepSize) {
        uint32 start = 0;
        if (i > 0)
            start = _curveEnds[i - 1];

        for (uint32 t = start + 2; t < _curveEnds[i]; ++t) {
            const Vec4f &p0 = _nodeData[t - 2];
            const Vec4f &p1 = _nodeData[t - 1];
            const Vec4f &p2 = _nodeData[t - 0];
            const Vec3f &n0 = _nodeNormals[t - 2];
            const Vec3f &n1 = _nodeNormals[t - 1];
            const Vec3f &n2 = _nodeNormals[t - 0];

            for (int j = 0; j <= samples; ++j) {
                float curveT = j*(1.0f/samples);
                Vec3f tangent = BSpline::quadraticDeriv(p0.xyz(), p1.xyz(), p2.xyz(), curveT).normalized();
                Vec3f normal = BSpline::quadratic(n0, n1, n2, curveT);
                Vec3f binormal = tangent.cross(normal).normalized();
                Vec4f p = BSpline::quadratic(p0, p1, p2, curveT);
                Vec3f v0 = -p.w()*binormal + p.xyz();
                Vec3f v1 =  p.w()*binormal + p.xyz();

                verts.emplace_back(v0);
                verts.emplace_back(v1);
                idx += 2;
                if (j > 0) {
                    tris.emplace_back(idx - 3, idx - 2, idx - 1);
                    tris.emplace_back(idx - 4, idx - 2, idx - 3);
                }
            }
        }
    }

    _proxy = std::make_shared<TriangleMesh>(verts, tris, _bsdf, "Curves", false, false);
}

void Curves::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);
    if (auto path = value["file"]) _path = scene.fetchResource(path);
    if (auto bsdf = value["bsdf"]) _bsdf = scene.fetchBsdf(bsdf);
    _mode = value["mode"];
    value.getField("curve_taper", _taperThickness);
    value.getField("subsample", _subsample);
    _overrideThickness = value.getField("curve_thickness", _curveThickness);
}

rapidjson::Value Curves::toJson(Allocator &allocator) const
{
    JsonObject result{Primitive::toJson(allocator), allocator,
        "type", "curves",
        "curve_taper", _taperThickness,
        "subsample", _subsample,
        "mode", _mode.toString(),
        "bsdf", *_bsdf
    };
    if (_path)
        result.add("file", *_path);
    if (_overrideThickness)
        result.add("curve_thickness", _curveThickness);

    return result;
}

void Curves::loadResources()
{
    loadCurves();
}

void Curves::saveResources()
{
    if (_path)
        saveAs(*_path);
}

void Curves::saveAs(const Path &path)
{
    CurveIO::CurveData data;
    data.curveEnds = &_curveEnds;
    data.nodeData  = &_nodeData;
    data.nodeColor = &_nodeColor;

    CurveIO::save(path, data);
}

bool Curves::intersect(Ray &ray, IntersectionTemporary &data) const
{
    if (_mode == MODE_RIBBON)
        return intersectTemplate<true>(ray, data);
    else
        return intersectTemplate<false>(ray, data);
}

template<bool isRibbon>
bool Curves::intersectTemplate(Ray &ray, IntersectionTemporary &data) const
{
    Vec3f o(ray.pos()), lz(ray.dir());
    float d = std::sqrt(lz.x()*lz.x() + lz.z()*lz.z());
    Vec3f lx, ly;
    if (d == 0.0f) {
        lx = Vec3f(1.0f, 0.0f, 0.0f);
        ly = Vec3f(0.0f, 0.0f, -lz.y());
    } else {
        lx = Vec3f(lz.z()/d, 0.0f, -lz.x()/d);
        ly = Vec3f(lx.z()*lz.y(), d, -lz.y()*lx.x());
    }

    bool didIntersect = false;
    CurveIntersection &isect = *data.as<CurveIntersection>();

    _bvh->trace(ray, [&](Ray &ray, uint32 id, float /*tMin*/, const Vec3pf &/*bounds*/) {
        Vec4f q0(project(o, lx, ly, lz, _nodeData[id - 2]));
        Vec4f q1(project(o, lx, ly, lz, _nodeData[id - 1]));
        Vec4f q2(project(o, lx, ly, lz, _nodeData[id - 0]));

        Vec3f n0, n1, n2;
        if (isRibbon) {
            n0 = project(lx, ly, lz, _nodeNormals[id - 2]);
            n1 = project(lx, ly, lz, _nodeNormals[id - 1]);
            n2 = project(lx, ly, lz, _nodeNormals[id - 0]);
        }

        if (pointOnSpline<isRibbon>(q0, q1, q2, ray.nearT(), ray.farT(), isect, n0, n1, n2)) {
            ray.setFarT(isect.t);
            isect.curveP0 = id - 2;
            didIntersect = true;
        }
    });

    if (didIntersect)
        data.primitive = this;

    return didIntersect;
}

bool Curves::occluded(const Ray &ray) const
{
    IntersectionTemporary tmp;
    Ray r(ray);
    return intersect(r, tmp);
}

bool Curves::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void Curves::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const CurveIntersection &isect = *data.as<CurveIntersection>();

    uint32 p0 = isect.curveP0;
    float t = isect.uv.x();

    Vec3f tangent = BSpline::quadraticDeriv(_nodeData[p0].xyz(), _nodeData[p0 + 1].xyz(), _nodeData[p0 + 2].xyz(), t);
    tangent.normalize();

    if (_mode == MODE_RIBBON) {
        Vec3f normal = BSpline::quadratic(_nodeNormals[p0], _nodeNormals[p0 + 1], _nodeNormals[p0 + 2], t);
        info.Ng = info.Ns = (tangent*tangent.dot(normal) - normal).normalized();
    } else if (_mode == MODE_BCSDF_CYLINDER) {
        info.Ng = info.Ns = (-info.w - tangent*tangent.dot(-info.w)).normalized();
    } else if (_mode == MODE_HALF_CYLINDER || _mode == MODE_CYLINDER) {
        Vec3f point = BSpline::quadratic(_nodeData[p0].xyz(), _nodeData[p0 + 1].xyz(), _nodeData[p0 + 2].xyz(), t);

        Vec3f localP = info.p - point;
        localP -= tangent*(localP.dot(tangent));
        info.Ng = info.Ns = localP.normalized();
    }

    info.uv = isect.uv;
    info.primitive = this;
    info.bsdf = _bsdf.get();

    if (_mode == MODE_CYLINDER)
        info.epsilon = max(info.epsilon, 0.1f*isect.w);
    else
        info.epsilon = max(info.epsilon, 0.01f*isect.w);
}

bool Curves::tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
        Vec3f &T, Vec3f &B) const
{
    const CurveIntersection &isect = *data.as<CurveIntersection>();

    uint32 p0 = isect.curveP0;
    float t = isect.uv.x();
    Vec3f tangent = BSpline::quadraticDeriv(_nodeData[p0].xyz(), _nodeData[p0 + 1].xyz(), _nodeData[p0 + 2].xyz(), t);

    B = tangent.normalized();
    T = B.cross(info.Ng);
    return true;
}

bool Curves::isSamplable() const
{
    return false;
}

void Curves::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Curves::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Curves::isDirac() const
{
    return _nodeCount == 0 || _curveCount == 0;
}

bool Curves::isInfinite() const
{
    return false;
}

float Curves::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return -1.0f;
}

Box3f Curves::bounds() const
{
    return _bounds;
}

const TriangleMesh &Curves::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Curves::prepareForRender()
{
    Bvh::PrimVector prims;
    prims.reserve(_nodeCount - 2*_curveCount);

    float widthScale = _transform.extractScaleVec().avg();

    for (Vec4f &data : _nodeData) {
        Vec3f newP = _transform*data.xyz();
        data.x() = newP.x();
        data.y() = newP.y();
        data.z() = newP.z();
        data.w() *= widthScale;
    }

    UniformSampler rand;
    for (uint32 i = 0; i < _curveCount; ++i) {
        uint32 start = 0;
        if (i > 0)
            start = _curveEnds[i - 1];

        if (_subsample > 0.0f && rand.next1D() < _subsample)
            continue;

        for (uint32 t = start + 2; t < _curveEnds[i]; ++t) {
            const Vec4f &p0 = _nodeData[t - 2];
            const Vec4f &p1 = _nodeData[t - 1];
            const Vec4f &p2 = _nodeData[t - 0];

            prims.emplace_back(
                curveBox(p0, p1, p2),
                (p0.xyz() + p1.xyz() + p2.xyz())*(1.0f/3.0f),
                t
            );
        }
    }

    _bvh.reset(new Bvh::BinaryBvh(std::move(prims), 2));

    //_needsRayTransform = true;

    computeBounds();

    Primitive::prepareForRender();
}

void Curves::teardownAfterRender()
{
    _bvh.reset();
    // TODO
    loadCurves();

    Primitive::teardownAfterRender();
}


int Curves::numBsdfs() const
{
    return 1;
}

std::shared_ptr<Bsdf> &Curves::bsdf(int /*index*/)
{
    return _bsdf;
}

void Curves::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &bsdf)
{
    _bsdf = bsdf;
}

Primitive *Curves::clone()
{
    return new Curves(*this);
}

}
