#include "Curves.hpp"
#include "TriangleMesh.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/BSpline.hpp"
#include "math/Vec.hpp"

#include "io/FileUtils.hpp"
#include "io/CurveIO.hpp"

namespace Tungsten {

struct CurveIntersection
{
    uint32 curveP0;
    float t;
    Vec2f uv;
    float w;
};

struct StackNode
{
    Vec2f p0, p1;
    float w0, w1;
    float tMin, tSpan;
    int depth;

    void set(float tMin_, float tSpan_, Vec2f p0_, Vec2f p1_, float w0_, float w1_, int depth_)
    {
        p0 = p0_;
        p1 = p1_;
        w0 = w0_;
        w1 = w1_;
        tMin = tMin_;
        tSpan = tSpan_;
        depth = depth_;
    }
};

static inline void intersectSegment(StackNode node, Vec4f q0,
        Vec4f q1, Vec4f q2, float tMin, float tMax,
        float &closestDepth, CurveIntersection &isect)
{
    float d0 = q0.z(), d1 = q1.z(), d2 = q2.z();
    float w0 = q0.w(), w1 = q1.w(), w2 = q2.w();

    Vec2f v = (node.p1 - node.p0);
    float lengthSq = v.lengthSq();
    float segmentT = -(node.p0.dot(v))/lengthSq;
    float distance;
    float signedUnnormalized = node.p0.x()*v.y() - node.p0.y()*v.x();
    if (segmentT <= 0.0f)
        distance = node.p0.length();
    else if (segmentT >= 1.0f)
        distance = node.p1.length();
    else
        distance = std::fabs(signedUnnormalized)/std::sqrt(lengthSq);

    float newT = segmentT*node.tSpan + node.tMin;
    float currentWidth = BSpline::quadratic(w0, w1, w2, newT);
    float currentDepth = BSpline::quadratic(d0, d1, d2, newT);
    if (currentDepth < tMax && currentDepth > tMin && distance < currentWidth && newT >= 0.0f && newT <= 1.0f) {
        float halfDistance = 0.5f*distance/currentWidth;
        isect.uv = Vec2f(newT, signedUnnormalized < 0.0f ? 0.5f - halfDistance : halfDistance + 0.5f);
        isect.t = currentDepth;
        isect.w = currentWidth;
        closestDepth = currentDepth;
    }
}

// Implementation of "Ray tracing for curves primitive" by Nakamaru and Ohno
// http://wscg.zcu.cz/wscg2002/Papers_2002/A83.pdf
static bool pointOnSpline(Vec4f q0, Vec4f q1, Vec4f q2, float tMin, float tMax, CurveIntersection &isect)
{
    CONSTEXPR int MaxDepth = 5;

    StackNode stackBuf[MaxDepth];
    StackNode *stack = &stackBuf[0];

    Vec2f p0 = q0.xy(), p1 = q1.xy(), p2 = q2.xy();
    float w0 = q0.w(),  w1 = q1.w(),  w2 = q2.w();

    Vec2f tFlat = (p0 - p1)/(p0 - 2.0f*p1 + p2);
    float xFlat = BSpline::quadratic(p0.x(), p1.x(), p2.x(), tFlat.x());
    float yFlat = BSpline::quadratic(p0.y(), p1.y(), p2.y(), tFlat.y());

    Vec2f deriv1(p0 - 2.0f*p1 + p2), deriv2(p1 - p0);

    StackNode cur{
        (p0 + p1)*0.5f,
        (p1 + p2)*0.5f,
        (w0 + w1)*0.5f,
        (w1 + w2)*0.5f,
        0.0f, 1.0f, 0
    };

    float closestDepth = tMax;

    while (true) {
        Vec2f pMin = min(cur.p0, cur.p1);
        Vec2f pMax = max(cur.p0, cur.p1);
        if (tFlat.x() > cur.tMin && tFlat.x() < cur.tMin + cur.tSpan) {
            pMin.x() = min(pMin.x(), xFlat);
            pMax.x() = max(pMax.x(), xFlat);
        }
        if (tFlat.y() > cur.tMin && tFlat.y() < cur.tMin + cur.tSpan) {
            pMin.y() = min(pMin.y(), yFlat);
            pMax.y() = max(pMax.y(), yFlat);
        }

        float maxWidth = max(cur.w0, cur.w1);
        if (pMin.x() <= maxWidth && pMin.y() <= maxWidth && pMax.x() >= -maxWidth && pMax.y() >= -maxWidth) {
            if (cur.depth >= MaxDepth) {
                Vec2f tangent0 = deriv2 + deriv1*cur.tMin;
                Vec2f tangent1 = deriv2 + deriv1*(cur.tMin + cur.tSpan);

                if (tangent0.dot(cur.p0) <= 0.0f && tangent1.dot(cur.p1) >= 0.0f)
                    intersectSegment(cur, q0, q1, q2, tMin, tMax, closestDepth, isect);
            } else {
                float newSpan = cur.tSpan*0.5f;
                float splitT = cur.tMin + newSpan;
                Vec4f qSplit = BSpline::quadratic(q0, q1, q2, splitT);

                (*stack++).set(cur.tMin, newSpan, cur.p0, qSplit.xy(), cur.w0, qSplit.w(), cur.depth + 1);
                cur.set(splitT, newSpan, qSplit.xy(), cur.p1, qSplit.w(), cur.w1, cur.depth + 1);
                continue;
            }
        }
        if (stack == stackBuf)
            break;
        cur = *--stack;
    }

    return closestDepth < tMax;
}

static Vec4f project(const Vec3f &o, const Vec3f &lx, const Vec3f &ly, const Vec3f &lz, const Vec4f &q)
{
    Vec3f p(q.xyz() - o);
    return Vec4f(lx.dot(p), ly.dot(p), lz.dot(p), q.w());
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

Curves::Curves(const Curves &o)
: Primitive(o)
{
    _path       = o._path;
    _curveCount = o._curveCount;
    _nodeCount  = o._nodeCount;
    _curveEnds  = o._curveEnds;
    _nodeData   = o._nodeData;
    _nodeColor  = o._nodeColor;
    _proxy      = o._proxy;
    _bounds     = o._bounds;
}

void Curves::loadCurves()
{
    CurveIO::CurveData data;
    data.curveEnds = &_curveEnds;
    data.nodeData  = &_nodeData;
    data.nodeColor = &_nodeColor;

    CurveIO::load(_path, data);

    _nodeCount = _nodeData.size();
    _curveCount = _curveEnds.size();
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

    const int Samples = _curveCount < 100 ? 100 : (_curveCount < 10000 ? 5 : 1);
    const int StepSize = _curveCount < 10000 ? 1 : 10;

    uint32 idx = 0;
    for (uint32 i = 0; i < _curveCount; i += StepSize) {
        uint32 start = 0;
        if (i > 0)
            start = _curveEnds[i - 1];

        for (uint32 t = start + 2; t < _curveEnds[i]; ++t) {
            const Vec4f &p0 = _nodeData[t - 2];
            const Vec4f &p1 = _nodeData[t - 1];
            const Vec4f &p2 = _nodeData[t - 0];

            for (int j = 0; j <= Samples; ++j) {
                float curveT = j*(1.0f/Samples);
                Vec3f tangent = BSpline::quadraticDeriv(p0.xyz(), p1.xyz(), p2.xyz(), curveT).normalized();
                TangentFrame frame(tangent);
                Vec4f p = BSpline::quadratic(p0, p1, p2, curveT);
                Vec3f v0 = frame.toGlobal(Vec3f(-p.w(), 0.0f, 0.0f)) + p.xyz();
                Vec3f v1 = frame.toGlobal(Vec3f( p.w(), 0.0f, 0.0f)) + p.xyz();

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

    _proxy = std::make_shared<TriangleMesh>(verts, tris, _bsdf, "Curves", false);
}

Vec3f Curves::computeTangent(const CurveIntersection &isect) const
{
    uint32 p0 = isect.curveP0;
    Vec3f tangent = BSpline::quadraticDeriv(
        _nodeData[p0].xyz(),
        _nodeData[p0 + 1].xyz(),
        _nodeData[p0 + 2].xyz(),
        isect.uv.x()
    );

    return tangent.normalized();
}

void Curves::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
    _path = JsonUtils::as<std::string>(v, "file");
    _dir = FileUtils::getCurrentDir();

    loadCurves();
}

rapidjson::Value Curves::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "curves", allocator);
    v.AddMember("file", _path.c_str(), allocator);
    return std::move(v);
}

bool Curves::intersect(Ray &ray, IntersectionTemporary &data) const
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

    _bvh->trace(ray, [&](Ray &ray, uint32 id) {
        Vec4f q0(project(o, lx, ly, lz, _nodeData[id - 2]));
        Vec4f q1(project(o, lx, ly, lz, _nodeData[id - 1]));
        Vec4f q2(project(o, lx, ly, lz, _nodeData[id - 0]));

        if (pointOnSpline(q0, q1, q2, ray.nearT(), ray.farT(), isect)) {
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
    Vec3f tangent = computeTangent(isect);
    info.Ng = info.Ns = (-info.w - tangent*tangent.dot(-info.w)).normalized();
    info.uv = isect.uv;
    info.primitive = this;
    info.epsilon = 10.0f*isect.w;
}

bool Curves::tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
        Vec3f &T, Vec3f &B) const
{
    const CurveIntersection &isect = *data.as<CurveIntersection>();
    T = computeTangent(isect);
    B = T.cross(info.Ng);
    return false;
}

bool Curves::isSamplable() const
{
    return false;
}

void Curves::makeSamplable()
{
}

float Curves::inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    return 0.0f;
}

bool Curves::sampleInboundDirection(LightSample &/*sample*/) const
{
    return false;
}

bool Curves::sampleOutboundDirection(LightSample &/*sample*/) const
{
    return false;
}

bool Curves::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Curves::isDelta() const
{
    return false;
}

bool Curves::isInfinite() const
{
    return false;
}

float Curves::approximateRadiance(const Vec3f &/*p*/) const
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
    std::vector<BvhBuilder<2>::Primitive> prims;
    prims.reserve(_nodeCount - 2*_curveCount);

    float widthScale = _transform.extractScaleVec().avg();

    for (Vec4f &data : _nodeData) {
        Vec3f newP = _transform*data.xyz();
        data.x() = newP.x();
        data.y() = newP.y();
        data.z() = newP.z();
        data.w() *= widthScale;
    }

    for (uint32 i = 0; i < _curveCount; ++i) {
        uint32 start = 0;
        if (i > 0)
            start = _curveEnds[i - 1];

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

    _bvh.reset(new BinaryBvh(std::move(prims), 2));

    //_needsRayTransform = true;

    computeBounds();
}

void Curves::cleanupAfterRender()
{
    _bvh.reset();
    std::string dir = FileUtils::getCurrentDir();
    FileUtils::changeCurrentDir(_dir);
    // TODO
    loadCurves();
    FileUtils::changeCurrentDir(dir);
}


Primitive *Curves::clone()
{
    return new Curves(*this);
}

void Curves::saveData()
{
    CurveIO::CurveData data;
    data.curveEnds = &_curveEnds;
    data.nodeData  = &_nodeData;
    data.nodeColor = &_nodeColor;

    CurveIO::save(_path, data);
}

}
