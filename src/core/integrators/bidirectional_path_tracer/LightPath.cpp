#include "LightPath.hpp"

#include "integrators/TraceState.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/TraceableScene.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "cameras/Camera.hpp"

namespace Tungsten {

float LightPath::misWeight(const LightPath &camera, const LightPath &emitter,
            const PathEdge &edge, int s, int t)
{
    float *pdfForward  = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    bool  *connectable = reinterpret_cast<bool  *>(alloca((s + t)*sizeof(bool)));

    for (int i = 0; i < s; ++i) {
        pdfForward [i] = emitter[i].pdfForward();
        pdfBackward[i] = emitter[i].pdfBackward();
        connectable[i] = emitter[i].connectable();
    }
    for (int i = 0; i < t; ++i) {
        pdfForward [s + t - (i + 1)] = camera[i].pdfBackward();
        pdfBackward[s + t - (i + 1)] = camera[i].pdfForward();
        connectable[s + t - (i + 1)] = camera[i].connectable();
    }

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
        if (connectable[i - 1] && connectable[i])
            weight += pi;
    }
    pi = 1.0f;
    for (int i = s - 1; i >= 1; --i) {
        pi *= pdfBackward[i]/pdfForward[i];
        if (connectable[i - 1] && connectable[i])
            weight += pi;
    }
    if (!emitter[0].emitter()->isDelta())
        weight += pi*pdfBackward[0]/pdfForward[0];

    return 1.0f/weight;
}

void LightPath::tracePath(const TraceableScene &scene, TraceBase &tracer, SampleGenerator &sampler,
        SampleGenerator &supplementalSampler)
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

Vec3f LightPath::bdptWeightedPathEmission(int minLength, int maxLength) const
{
    // TODO: Naive, slow version to make sure it's correct. Optimize this

    float *pdfForward  = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    bool  *connectable = reinterpret_cast<bool  *>(alloca(_length*sizeof(bool)));

    Vec3f result(0.0f);
    for (int t = max(minLength, 2); t <= min(_length, maxLength); ++t) {
        Vec3f emission = _vertices[t - 1].surfaceRecord().info.primitive->emission(
                _vertices[t - 1].surfaceRecord().data,
                _vertices[t - 1].surfaceRecord().info);
        if (emission == 0.0f)
            continue;

        for (int i = 0; i < t; ++i) {
            pdfForward [t - (i + 1)] = _vertices[i].pdfBackward();
            pdfBackward[t - (i + 1)] = _vertices[i].pdfForward();
            connectable[t - (i + 1)] = _vertices[i].connectable();
        }
        connectable[0] = true;

        PositionSample point(_vertices[t - 1].surfaceRecord().info);
        DirectionSample direction(-_edges[t - 2].d);
        pdfForward[0] = _vertices[t - 1].surfaceRecord().info.primitive->positionalPdf(point);
        pdfForward[1] = _vertices[t - 1].surfaceRecord().info.primitive->directionalPdf(point, direction)*
                _vertices[t - 2].cosineFactor(_edges[t - 2].d)/_edges[t - 2].rSq;

        float weight = 1.0f;
        float pi = 1.0f;
        for (int i = 1; i < t; ++i) {
            pi *= pdfForward[i - 1]/pdfBackward[i - 1];
            if (connectable[i - 1] && connectable[i])
                weight += pi;
        }
        result += emission/weight;
    }

    return result;
}

Vec3f LightPath::bdptConnect(const TraceableScene &scene, const LightPath &camera,
        const LightPath &emitter, int s, int t)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[t - 1];

    PathEdge edge(a, b);
    if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
        return Vec3f(0.0f);

    Vec3f unweightedContrib = a.throughput()*a.eval(edge.d)*b.eval(-edge.d)*b.throughput()/edge.rSq;

    return unweightedContrib*misWeight(camera, emitter, edge, s, t);
}

bool LightPath::bdptCameraConnect(const TraceableScene &scene, const LightPath &camera,
        const LightPath &emitter, int s, SampleGenerator &sampler,
        Vec3f &weight, Vec2u &pixel)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[0];

    PathEdge edge(a, b);
    if (scene.occluded(Ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f))))
        return false;

    Vec3f splatWeight;
    if (!b.camera()->evalDirection(sampler, b.cameraRecord().point, DirectionSample(-edge.d), splatWeight, pixel))
        return false;

    weight = splatWeight*b.throughput()*a.eval(edge.d)*a.throughput()/edge.rSq;
    weight *= misWeight(camera, emitter, edge, s, 1);

    return true;
}

void LightPath::samplePathsInterleaved(LightPath &cameraPath, LightPath &emitterPath,
        const TraceableScene &scene, TraceBase &tracer, SampleGenerator &sampler,
        SampleGenerator &supplementalSampler)
{
    TraceState  cameraState(sampler, supplementalSampler);
    TraceState emitterState(sampler, supplementalSampler);
    bool  cameraPathValid =  cameraPath[0].sampleRootVertex( cameraState);
    bool emitterPathValid = emitterPath[0].sampleRootVertex(emitterState);

    int  cameraLength = 0;
    int emitterLength = 0;
    while (cameraPathValid || emitterPathValid) {
        if (cameraPathValid) {
            cameraLength++;
            cameraPathValid = cameraLength < cameraPath.maxLength() &&
                    cameraPath[cameraLength - 1].sampleNextVertex(scene, tracer, cameraState,
                            cameraLength == 1 ? nullptr : &cameraPath[cameraLength - 2],
                            cameraLength == 1 ? nullptr : &cameraPath.edge(cameraLength - 2),
                            cameraPath[cameraLength], cameraPath.edge(cameraLength - 1));
        }
        if (emitterPathValid) {
            emitterLength++;
            emitterPathValid = emitterLength < emitterPath.maxLength() &&
                    emitterPath[emitterLength - 1].sampleNextVertex(scene, tracer, emitterState,
                            emitterLength == 1 ? nullptr : &emitterPath[emitterLength - 2],
                            emitterLength == 1 ? nullptr : &emitterPath.edge(emitterLength - 2),
                            emitterPath[emitterLength], emitterPath.edge(emitterLength - 1));
        }
    }
     cameraPath._length =  cameraLength;
    emitterPath._length = emitterLength;
}

}
