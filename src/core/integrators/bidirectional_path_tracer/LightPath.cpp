#include "LightPath.hpp"

#include "integrators/TraceState.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/TraceableScene.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "cameras/Camera.hpp"

namespace Tungsten {

void LightPath::tracePath(const TraceableScene &scene, TraceBase &tracer, SampleGenerator &sampler,
        UniformSampler &supplementalSampler)
{
    TraceState state(sampler, supplementalSampler);
    if (!_vertices[0].sampleRootVertex(state))
        return;

    _length = 1;
    while (state.bounce < _maxLength) {
        if (!_vertices[_length - 1].sampleNextVertex(scene, tracer, state,
                _length == 1 ? nullptr : &_vertices[_length - 2],
                _length == 1 ? nullptr : &_edges[_length - 2],
                _vertices[_length], _edges[_length - 1]))
            break;
        _length++;
    }
}

Vec3f LightPath::weightedPathEmission(int minLength, int maxLength) const
{
    // TODO: Naive, slow version to make sure it's correct. Optimize this

    float *pdfForward    = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    float *pdfBackward   = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    bool  *isConnectable = reinterpret_cast<bool  *>(alloca(_length*sizeof(bool)));

    Vec3f result(0.0f);
    for (int t = max(minLength, 2); t <= min(_length, maxLength); ++t) {
        Vec3f emission = _vertices[t - 1]._record.surface.info.primitive->emission(_vertices[t - 1]._record.surface.data,
                _vertices[t - 1]._record.surface.info);
        if (emission == 0.0f)
            continue;

        for (int i = 0; i < t; ++i) {
            pdfForward   [t - (i + 1)] = _vertices[i].pdfBackward;
            pdfBackward  [t - (i + 1)] = _vertices[i].pdfForward;
            isConnectable[t - (i + 1)] = _vertices[i].isConnectable;
        }
        isConnectable[0] = true;

        PositionSample point(_vertices[t - 1]._record.surface.info);
        DirectionSample direction(-_edges[t - 2].d);
        pdfForward[0] = _vertices[t - 1]._record.surface.info.primitive->positionalPdf(point);
        pdfForward[1] = _vertices[t - 1]._record.surface.info.primitive->directionalPdf(point, direction)*
                _vertices[t - 2].cosineFactor(_edges[t - 2].d)/_edges[t - 2].rSq;

        float weight = 1.0f;
        float pi = 1.0f;
        for (int i = 1; i < t; ++i) {
            pi *= pdfForward[i - 1]/pdfBackward[i - 1];
            if (isConnectable[i - 1] && isConnectable[i])
                weight += pi;
        }
        result += emission/weight;
    }

    return result;
}

Vec3f LightPath::connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b)
{
    PathEdge edge(a, b);
    if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
        return Vec3f(0.0f);

    return a.throughput*a.eval(edge.d)*b.eval(-edge.d)*b.throughput/edge.rSq;
}

bool LightPath::connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b,
        SampleGenerator &sampler, Vec3f &weight, Vec2u &pixel)
{
    PathEdge edge(a, b);
    if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
        return false;

    Vec3f splatWeight;
    if (!a.sampler.camera->evalDirection(sampler, a._record.camera.point, DirectionSample(edge.d), splatWeight, pixel))
        return false;

    weight = splatWeight*a.throughput*b.eval(-edge.d)*b.throughput/edge.rSq;

    return true;
}

float LightPath::misWeight(const LightPath &camera, const LightPath &emitter, int s, int t)
{
    float *pdfForward    = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    float *pdfBackward   = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    bool  *isConnectable = reinterpret_cast<bool  *>(alloca((s + t)*sizeof(bool)));

    for (int i = 0; i < s; ++i) {
        pdfForward   [i] = emitter[i].pdfForward;
        pdfBackward  [i] = emitter[i].pdfBackward;
        isConnectable[i] = emitter[i].isConnectable;
    }
    for (int i = 0; i < t; ++i) {
        pdfForward   [s + t - (i + 1)] = camera[i].pdfBackward;
        pdfBackward  [s + t - (i + 1)] = camera[i].pdfForward;
        isConnectable[s + t - (i + 1)] = camera[i].isConnectable;
    }

    PathEdge edge(emitter[s - 1], camera[t - 1]);
    emitter[s - 1].evalPdfs(s == 1 ? nullptr : &emitter[s - 2],
                            s == 1 ? nullptr : &emitter.edge(s - 2),
                            camera[t - 1], edge, &pdfForward[s],
                            s == 1 ? nullptr : &pdfBackward[s - 2]);
    camera[t - 1].evalPdfs(t == 1 ? nullptr : &camera[t - 2],
                           t == 1 ? nullptr : &camera.edge(t - 2),
                           emitter[s - 1], edge.reverse(), &pdfBackward[s - 1],
                           t == 1 ? nullptr : &pdfForward[s + 1]);

    float weight = 1.0f;
    float pi = 1.0f;
    for (int i = s + 1; i < s + t; ++i) {
        pi *= pdfForward[i - 1]/pdfBackward[i - 1];
        if (isConnectable[i - 1] && isConnectable[i])
            weight += pi;
    }
    pi = 1.0f;
    for (int i = s - 1; i >= 1; --i) {
        pi *= pdfBackward[i]/pdfForward[i];
        if (isConnectable[i - 1] && isConnectable[i])
            weight += pi;
    }
    if (!emitter[0].sampler.emitter->isDelta())
        weight += pi*pdfBackward[0]/pdfForward[0];

    return 1.0f/weight;
}

}
