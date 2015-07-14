#include "LightPath.hpp"

#include "integrators/TraceState.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/TraceableScene.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "cameras/Camera.hpp"

namespace Tungsten {

float LightPath::geometryFactor(int startVertex) const
{
    const PathEdge &edge = _edges[startVertex];
    const PathVertex &v0 = _vertices[startVertex + 0];
    const PathVertex &v1 = _vertices[startVertex + 1];
    return v0.cosineFactor(edge.d)*v1.cosineFactor(edge.d)/edge.rSq;
}

float LightPath::invGeometryFactor(int startVertex) const
{
    const PathEdge &edge = _edges[startVertex];
    const PathVertex &v0 = _vertices[startVertex + 0];
    const PathVertex &v1 = _vertices[startVertex + 1];
    return edge.rSq/(v0.cosineFactor(edge.d)*v1.cosineFactor(edge.d));
}

float LightPath::misWeight(const LightPath &camera, const LightPath &emitter,
            const PathEdge &edge, int s, int t)
{
    float *pdfForward  = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    bool  *connectable = reinterpret_cast<bool  *>(alloca((s + t)*sizeof(bool)));

    for (int i = 0; i < s; ++i) {
        pdfForward [i] = emitter[i].pdfForward();
        pdfBackward[i] = emitter[i].pdfBackward();
        connectable[i] = !emitter[i].isDirac();
    }
    for (int i = 0; i < t; ++i) {
        pdfForward [s + t - (i + 1)] = camera[i].pdfBackward();
        pdfBackward[s + t - (i + 1)] = camera[i].pdfForward();
        connectable[s + t - (i + 1)] = !camera[i].isDirac();
    }
    connectable[s - 1] = connectable[s] = true;

    emitter[s - 1].evalPdfs(s == 1 ? nullptr : &emitter[s - 2],
                            s == 1 ? nullptr : &emitter.edge(s - 2),
                            camera[t - 1], edge, &pdfForward[s],
                            s == 1 ? nullptr : &pdfBackward[s - 2]);
    camera[t - 1].evalPdfs(t == 1 ? nullptr : &camera[t - 2],
                           t == 1 ? nullptr : &camera.edge(t - 2),
                           emitter[s - 1], edge.reverse(), &pdfBackward[s - 1],
                           t == 1 ? nullptr : &pdfForward[s + 1]);

    // Convert densities of dirac vertices sampled from non-dirac vertices to projected solid angle measure
    if (connectable[0] && !connectable[1] && !emitter[0].isInfiniteEmitter())
        pdfForward[1] *= emitter.invGeometryFactor(0);
    for (int i = 1; i < s + t - 1; ++i)
        if (connectable[i] && !connectable[i + 1])
            pdfForward[i + 1] *= (i < s) ? emitter.invGeometryFactor(i) : camera.invGeometryFactor(s + t - 2 - i);
    for (int i = s + t - 1; i >= 1; --i)
        if (connectable[i] && !connectable[i - 1])
            pdfBackward[i - 1] *= (i < s) ? emitter.invGeometryFactor(i - 1) : camera.invGeometryFactor(s + t - 1 - i);

    float weight = 1.0f;
    float pi = 1.0f;
    for (int i = s + 1; i < s + t; ++i) {
        pi *= pdfForward[i - 1]/pdfBackward[i - 1];
        if (connectable[i - 1] && connectable[i])
            weight += pi;
    }
    pi = 1.0f;
    for (int i = s - 1; i >= 1; --i) {
        pi *= pdfBackward[i]/pdfForward[i];
        if (connectable[i - 1] && connectable[i])
            weight += pi;
    }
    if (!emitter[0].emitter()->isDirac())
        weight += pi*pdfBackward[0]/pdfForward[0];

    return 1.0f/weight;
}

void LightPath::tracePath(const TraceableScene &scene, TraceBase &tracer, PathSampleGenerator &sampler, int length)
{
    if (length == -1)
        length = _maxLength;

    TraceState state(sampler);
    if (!_vertices[0].sampleRootVertex(state))
        return;

    _length = 1;
    while (_length < length) {
        if (!_vertices[_length - 1].sampleNextVertex(scene, tracer, state, _adjoint,
                _length == 1 ? nullptr : &_vertices[_length - 2],
                _length == 1 ? nullptr : &_edges[_length - 2],
                _vertices[_length], _edges[_length - 1]))
            break;
        sampler.advancePath();
        _length++;
    }
}

Vec3f LightPath::bdptWeightedPathEmission(int minLength, int maxLength) const
{
    // TODO: Naive, slow version to make sure it's correct. Optimize this

    float *pdfForward  = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    bool  *connectable = reinterpret_cast<bool  *>(alloca(_length*sizeof(bool)));

    maxLength = min(_length, maxLength);

    // Early out for camera paths directly hitting the environment map
    // These can only be sampled with one technique
    if (maxLength == 2 && minLength <= 2 && _vertices[1].surfaceRecord().data.primitive->isInfinite())
        return _vertices[1].surfaceRecord().data.primitive->evalDirect(
                _vertices[1].surfaceRecord().data,
                _vertices[1].surfaceRecord().info);


    Vec3f result(0.0f);
    for (int t = max(minLength, 2); t <= maxLength; ++t) {
        const SurfaceRecord &record = _vertices[t - 1].surfaceRecord();
        if (!record.info.primitive->isEmissive())
            continue;

        Vec3f emission = record.info.primitive->evalDirect(
                _vertices[t - 1].surfaceRecord().data,
                _vertices[t - 1].surfaceRecord().info);
        if (emission == 0.0f)
            continue;

        for (int i = 0; i < t; ++i) {
            pdfForward [t - (i + 1)] = _vertices[i].pdfBackward();
            pdfBackward[t - (i + 1)] = _vertices[i].pdfForward();
            connectable[t - (i + 1)] = !_vertices[i].isDirac();
        }
        connectable[0] = true;

        // Convert densities of dirac vertices sampled from non-dirac vertices to projected solid angle measure
        if (connectable[0] && !connectable[1] && !_vertices[t - 1].isInfiniteSurface())
            pdfForward[1] *= invGeometryFactor(t - 2);
        for (int i = 1; i < t - 1; ++i)
            if (connectable[i] && !connectable[i + 1])
                pdfForward[i + 1] *= invGeometryFactor(t - 2 - i);
        for (int i = t - 1; i >= 1; --i)
            if (connectable[i] && !connectable[i - 1])
                pdfBackward[i - 1] *= invGeometryFactor(t - 1 - i);

        PositionSample point(record.info);
        DirectionSample direction(-_edges[t - 2].d);
        if (record.info.primitive->isInfinite()) {
            // Infinite primitives sample direction first before sampling a position
            // The PDF of the first vertex is also given in solid angle measure,
            // not area measure
            pdfForward[0] = record.info.primitive->directionalPdf(point, direction);
            pdfForward[1] = record.info.primitive->positionalPdf(point)*
                    _vertices[t - 2].cosineFactor(_edges[t - 2].d);
        } else {
            pdfForward[0] = record.info.primitive->positionalPdf(point);
            pdfForward[1] = record.info.primitive->directionalPdf(point, direction)*
                    _vertices[t - 2].cosineFactor(_edges[t - 2].d)/_edges[t - 2].rSq;
        }

        float weight = 1.0f;
        float pi = 1.0f;
        for (int i = 1; i < t; ++i) {
            pi *= pdfForward[i - 1]/pdfBackward[i - 1];
            if (connectable[i - 1] && connectable[i])
                weight += pi;
        }

        result += _vertices[t - 1].throughput()*emission/weight;
    }

    return result;
}

Vec3f LightPath::bdptConnect(const TraceableScene &scene, const LightPath &camera,
        const LightPath &emitter, int s, int t)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[t - 1];

    if (b.isInfiniteSurface())
        return Vec3f(0.0f);

    if (s == 1 && a.emitter()->isInfinite()) {
        // We do account for s=1, t>1 paths for infinite area lights
        // This essentially amounts to direct light sampling, which
        // we don't want to lose. This requires some fiddling with densities
        // and shadow rays here
        Vec3f d = a.emitterRecord().point.Ng;
        if (scene.occluded(Ray(b.pos(), -d, 1e-4f)))
            return Vec3f(0.0f);

        Vec3f unweightedContrib = a.throughput()*a.eval(d, true)*b.eval(-d, false)*b.throughput();

        return unweightedContrib*misWeight(camera, emitter, PathEdge(d, 1.0f, 1.0f), s, t);
    } else {
        PathEdge edge(a, b);
        // Catch the case where both vertices land on the same surface
        if (a.cosineFactor(edge.d) < 1e-5f || b.cosineFactor(edge.d) < 1e-5f)
            return Vec3f(0.0f);
        if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
            return Vec3f(0.0f);

        Vec3f unweightedContrib = a.throughput()*a.eval(edge.d, true)*b.eval(-edge.d, false)*b.throughput()/edge.rSq;

        return unweightedContrib*misWeight(camera, emitter, edge, s, t);
    }
}

bool LightPath::bdptCameraConnect(const TraceableScene &scene, const LightPath &camera,
        const LightPath &emitter, int s, PathSampleGenerator &sampler,
        Vec3f &weight, Vec2f &pixel)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[0];

    // s=1,t=1 paths are generally useless for infinite area lights, so we ignore them completely
    if (s == 1 && a.emitter()->isInfinite())
        return false;

    PathEdge edge(a, b);
    if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
        return false;

    Vec3f splatWeight;
    if (!b.camera()->evalDirection(sampler, b.cameraRecord().point, DirectionSample(-edge.d), splatWeight, pixel))
        return false;

    weight = splatWeight*b.throughput()*a.eval(edge.d, true)*a.throughput()/edge.rSq;
    weight *= misWeight(camera, emitter, edge, s, 1);

    return true;
}

}
