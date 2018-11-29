#include "LightPath.hpp"

#include "integrators/TraceState.hpp"
#include "integrators/TraceBase.hpp"

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

void LightPath::toAreaMeasure()
{
    // BDPT PDF wrangling starts here

    // Step 1: Forward events (transparency events, thinsheet glass,
    // index-matched dielectrics) are "punched through" by the generalized
    // shadow ray, so we don't treat them as diracs. Instead, we remove
    // them from the path entirely. Forward event chains are collapsed,
    // and the corresponding forward PDFs as well as intermittent
    // media PDFs are all folded into the forward- and backward
    // PDFs of the remaining edge. The remaining edge is extended
    // so that it has the combined length of all edges in the forward chain
    _vertexIndex[0] = 0;
    int insertionIdx = 1;
    for (int i = 1; i < _length; ++i) {
        if (_vertices[i].isForward()) {
            int tail = insertionIdx - 1;
            _edges[tail].r += _edges[i].r;
            _edges[tail].pdfForward  *= _edges[i].pdfForward *_vertices[i + 1].pdfForward();
            _edges[tail].pdfBackward *= _edges[i].pdfBackward*_vertices[tail ].pdfBackward();
            _vertices[tail ].pdfBackward() = _vertices[i].pdfBackward();
            _vertices[i + 1].pdfForward()  = _vertices[i].pdfForward();
        } else {
            _vertexIndex[insertionIdx] = i;
            _vertices[insertionIdx] = _vertices[i];
            _edges[insertionIdx] = _edges[i];
            insertionIdx++;
        }
    }
    _length = insertionIdx;

    // Step 2: Now that only "true" scattering events remain, we recompute the
    // squared distances (since the edge length may have changed) and multiply
    // the transmission PDFs stored on the edge onto the vertices
    // Since we moved some of the vertices, we also have to fix some pointers
    for (int i = 1; i < _length; ++i) {
        _edges[i - 1].rSq = _edges[i - 1].r*_edges[i - 1].r;
        _vertices[i]    .pdfForward()  *= _edges[i - 1].pdfForward;
        _vertices[i - 1].pdfBackward() *= _edges[i - 1].pdfBackward;
        if (_vertices[i].onSurface())
            _vertices[i].pointerFixup();
    }

    // Step 3: Now we have meaningful PDFs on the vertices, so we're ready to convert
    // (some of) them to area measure. Dirac vertices are left in the discrete measure,
    // and vertices associated with infinite area emitters are converted to solid angle measure
    for (int i = 1; i < _length; ++i) {
        if (_vertices[i - 1].isDirac() || _vertices[i].isInfiniteSurface())
            continue;
        if (_vertices[i].onSurface())
            _vertices[i].pdfForward() *= _vertices[i].cosineFactor(_edges[i - 1].d);
        if (!_vertices[i - 1].isInfiniteEmitter())
            _vertices[i].pdfForward() /= _edges[i - 1].rSq;
    }

    for (int i = _length - 3; i >= 0; --i) {
        if (_vertices[i + 1].isDirac() || _vertices[i].isInfiniteEmitter())
            continue;
        if (_vertices[i].onSurface())
            _vertices[i].pdfBackward() *= _vertices[i].cosineFactor(_edges[i].d);
        _vertices[i].pdfBackward() /= _edges[i].rSq;
    }
}

float LightPath::misWeight(const LightPath &camera, const LightPath &emitter,
            const PathEdge &edge, int s, int t, float *ratios)
{
    float *pdfForward           = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    float *pdfBackward          = reinterpret_cast<float *>(alloca((s + t)*sizeof(float)));
    bool  *connectable          = reinterpret_cast<bool  *>(alloca((s + t)*sizeof(bool)));
    const PathVertex **vertices = reinterpret_cast<const PathVertex **>(alloca((s + t)*sizeof(const PathVertex *)));

    if (!camera[t - 1].segmentConnectable(emitter[s - 1]))
        return 0.0f;

    for (int i = 0; i < s; ++i) {
        pdfForward [i] = emitter[i].pdfForward();
        pdfBackward[i] = emitter[i].pdfBackward();
        connectable[i] = !emitter[i].isDirac();
        vertices   [i] = &emitter[i];
    }
    for (int i = 0; i < t; ++i) {
        pdfForward [s + t - (i + 1)] = camera[i].pdfBackward();
        pdfBackward[s + t - (i + 1)] = camera[i].pdfForward();
        connectable[s + t - (i + 1)] = !camera[i].isDirac();
        vertices   [s + t - (i + 1)] = &camera[i];
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
    if (ratios)
        ratios[s] = 1.0f;
    for (int i = s + 1; i < s + t; ++i) {
        pi *= pdfForward[i - 1]/pdfBackward[i - 1];
        if (connectable[i - 1] && connectable[i] && vertices[i - 1]->segmentConnectable(*vertices[i])) {
            weight += pi;
            if (ratios)
                ratios[i] = pi;
        } else {
            if (ratios)
                ratios[i] = 0.0f;
        }
    }
    pi = 1.0f;
    for (int i = s - 1; i >= 1; --i) {
        pi *= pdfBackward[i]/pdfForward[i];
        if (connectable[i - 1] && connectable[i] && vertices[i - 1]->segmentConnectable(*vertices[i])) {
            weight += pi;
            if (ratios)
                ratios[i] = pi;
        } else {
            if (ratios)
                ratios[i] = 0.0f;
        }
    }
    if (!emitter[0].emitter()->isDirac()) {
        pi *= pdfBackward[0]/pdfForward[0];
        weight += pi;
        if (ratios)
            ratios[0] = pi;
    } else {
        if (ratios)
            ratios[0] = 0.0f;
    }

    return 1.0f/weight;
}

void LightPath::tracePath(const TraceableScene &scene, TraceBase &tracer, PathSampleGenerator &sampler, int length, bool prunePath)
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

    // Trim non-connectable vertices off the end, since they're not useful for anything
    while (_length > 0 && !_vertices[_length - 1].connectable())
        _length--;

    if (prunePath)
        prune();
}

void LightPath::prune()
{
    toAreaMeasure();
}

void LightPath::copy(const LightPath &o)
{
    _maxLength   = o._maxLength;
    _maxVertices = o._maxVertices;
    _length      = o._length;
    _adjoint     = o._adjoint;
    std::memcpy(_vertexIndex.get(), o._vertexIndex.get(), _maxVertices*sizeof(_vertexIndex[0]));
    std::memcpy(_vertices   .get(), o._vertices   .get(), _maxVertices*sizeof(_vertices   [0]));
    std::memcpy(_edges      .get(), o._edges      .get(), _maxVertices*sizeof(_edges      [0]));

    for (int i = 0; i < _maxVertices; ++i)
        if (_vertices[i].onSurface())
            _vertices[i].pointerFixup();
}

Vec3f LightPath::bdptWeightedPathEmission(int minLength, int maxLength, float *ratios, Vec3f *directEmissionByBounce) const
{
    // TODO: Naive, slow version to make sure it's correct. Optimize this

    float *pdfForward  = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca(_length*sizeof(float)));
    bool  *connectable = reinterpret_cast<bool  *>(alloca(_length*sizeof(bool)));

    if (directEmissionByBounce)
        std::memset(directEmissionByBounce, 0, (maxLength - 1)*sizeof(Vec3f));

    Vec3f result(0.0f);
    for (int t = 2; t <= _length; ++t) {
        int realT = _vertexIndex[t - 1] + 1;
        if (realT > maxLength)
            break;
        if (realT < minLength || !_vertices[t - 1].onSurface())
            continue;

        const SurfaceRecord &record = _vertices[t - 1].surfaceRecord();
        if (!record.info.primitive->isEmissive())
            continue;

        Vec3f emission = record.info.primitive->evalDirect(
                _vertices[t - 1].surfaceRecord().data,
                _vertices[t - 1].surfaceRecord().info);
        if (emission == 0.0f)
            continue;

        // Early out for camera paths directly hitting the environment map
        // These can only be sampled with one technique
        if (realT == 2 && _vertices[t - 1].isInfiniteSurface()) {
            Vec3f v = emission*_vertices[t - 1].throughput();
            if (directEmissionByBounce)
                directEmissionByBounce[0] = v;
            return v;
        }

        for (int i = 0; i < t; ++i) {
            pdfForward [t - (i + 1)] = _vertices[i].pdfBackward();
            pdfBackward[t - (i + 1)] = _vertices[i].pdfForward();
            connectable[t - (i + 1)] = !_vertices[i].isDirac();
        }
        connectable[0] = true;

        PositionSample point(record.info);
        DirectionSample direction(-_edges[t - 2].d);
        if (record.info.primitive->isInfinite()) {
            // Infinite primitives sample direction first before sampling a position
            // The PDF of the first vertex is also given in solid angle measure,
            // not area measure
            pdfForward[0] = record.info.primitive->directionalPdf(point, direction);
            pdfForward[1] = record.info.primitive->positionalPdf(point)*
                    _edges[t - 2].pdfBackward*_vertices[t - 2].cosineFactor(_edges[t - 2].d);
        } else {
            pdfForward[0] = record.info.primitive->positionalPdf(point);
            pdfForward[1] = record.info.primitive->directionalPdf(point, direction)*
                    _edges[t - 2].pdfBackward*_vertices[t - 2].cosineFactor(_edges[t - 2].d)/_edges[t - 2].rSq;
        }

        // Convert densities of dirac vertices sampled from non-dirac vertices to projected solid angle measure
        if (connectable[0] && !connectable[1] && !_vertices[t - 1].isInfiniteSurface())
            pdfForward[1] *= invGeometryFactor(t - 2);
        for (int i = 1; i < t - 1; ++i)
            if (connectable[i] && !connectable[i + 1])
                pdfForward[i + 1] *= invGeometryFactor(t - 2 - i);
        for (int i = t - 1; i >= 1; --i)
            if (connectable[i] && !connectable[i - 1])
                pdfBackward[i - 1] *= invGeometryFactor(t - 1 - i);

        float weight = 1.0f;
        float pi = 1.0f;
        if (ratios)
            ratios[0] = 1.0f;
        for (int i = 1; i < t; ++i) {
            pi *= pdfForward[i - 1]/pdfBackward[i - 1];
            if (connectable[i - 1] && connectable[i]) {
                weight += pi;
                if (ratios)
                    ratios[i] = pi;
            } else {
                if (ratios)
                    ratios[i] = 0.0f;
            }
        }

        Vec3f v = _vertices[t - 1].throughput()*emission/weight;
        if (directEmissionByBounce)
            directEmissionByBounce[t - 2] = v;
        result += v;
    }

    return result;
}

Vec3f LightPath::bdptConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
        int s, int t, int maxBounce, PathSampleGenerator &sampler, float *ratios)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[t - 1];

    int bounce = emitter.vertexIndex(s - 1) + camera.vertexIndex(t - 1);
    if (bounce >= maxBounce)
        return Vec3f(0.0f);

    if (b.isInfiniteSurface())
        return Vec3f(0.0f);

    if (s == 1 && a.emitter()->isInfinite()) {
        // We do account for s=1, t>1 paths for infinite area lights
        // This essentially amounts to direct light sampling, which
        // we don't want to lose. This requires some fiddling with densities
        // and shadow rays here
        Vec3f d = a.emitterRecord().point.Ng;
        PathEdge edge(d, 1.0f, 1.0f);
        Ray ray(b.pos(), -d, 1e-4f);
        Vec3f transmittance = tracer.generalizedShadowRayAndPdfs(sampler, ray, b.selectMedium(-d), nullptr,
                bounce, b.onSurface(), true, edge.pdfBackward, edge.pdfForward);
        if (transmittance == 0.0f)
            return Vec3f(0.0f);

        Vec3f unweightedContrib = transmittance*a.throughput()*a.eval(d, true)*b.eval(-d, false)*b.throughput();

        return unweightedContrib*misWeight(camera, emitter, edge, s, t, ratios);
    } else {
        PathEdge edge(a, b);
        // Catch the case where both vertices land on the same surface
        if (a.cosineFactor(edge.d) < 1e-5f || b.cosineFactor(edge.d) < 1e-5f)
            return Vec3f(0.0f);
        Ray ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f));
        Vec3f transmittance = tracer.generalizedShadowRayAndPdfs(sampler, ray, a.selectMedium(edge.d), nullptr,
                bounce, a.onSurface(), b.onSurface(), edge.pdfForward, edge.pdfBackward);
        if (transmittance == 0.0f)
            return Vec3f(0.0f);

        Vec3f unweightedContrib = transmittance*a.throughput()*a.eval(edge.d, true)*b.eval(-edge.d, false)*b.throughput()/edge.rSq;

        return unweightedContrib*misWeight(camera, emitter, edge, s, t, ratios);
    }
}

bool LightPath::bdptCameraConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
        int s, int maxBounce, PathSampleGenerator &sampler, Vec3f &weight, Vec2f &pixel, float *ratios)
{
    const PathVertex &a = emitter[s - 1];
    const PathVertex &b = camera[0];

    int bounce = emitter.vertexIndex(s - 1) + camera.vertexIndex(0);
    if (bounce >= maxBounce)
        return false;

    // s=1,t=1 paths are generally useless for infinite area lights, so we ignore them completely
    if (s == 1 && a.emitter()->isInfinite())
        return false;

    PathEdge edge(a, b);
    Ray ray(a.pos(), edge.d, 1e-4f, edge.r*(1.0f - 1e-4f));
    Vec3f transmittance = tracer.generalizedShadowRayAndPdfs(sampler, ray, a.selectMedium(edge.d), nullptr,
            bounce, a.onSurface(), b.onSurface(), edge.pdfForward, edge.pdfBackward);
    if (transmittance == 0.0f)
        return false;

    Vec3f splatWeight;
    if (!b.camera()->evalDirection(sampler, b.cameraRecord().point, DirectionSample(-edge.d), splatWeight, pixel))
        return false;

    weight = transmittance*splatWeight*b.throughput()*a.eval(edge.d, true)*a.throughput()/edge.rSq;
    weight *= misWeight(camera, emitter, edge, s, 1, ratios);

    return true;
}

bool LightPath::invert(WritablePathSampleGenerator &cameraSampler, WritablePathSampleGenerator &emitterSampler,
        const LightPath &camera, const LightPath &emitter, int newS)
{
    int s = emitter.length();
    int t = camera.length();
    int newT = s + t - newS;
    int vEnd = s + t - 1;
    int eEnd = s + t - 2;

    const PathVertex **vertices = reinterpret_cast<const PathVertex **>(alloca((s + t)*sizeof(const PathVertex *)));
    PathEdge *edges = reinterpret_cast<PathEdge *>(alloca((s + t - 1)*sizeof(PathEdge)));

    for (int i = 0; i < s; ++i)
        vertices[i] = &emitter[i];
    for (int i = 0; i < t; ++i)
        vertices[vEnd - i] = &camera[i];

    for (int i = 0; i < s - 1; ++i)
        edges[i] = emitter.edge(i);
    for (int i = 0; i < t - 1; ++i)
        edges[eEnd - i] = camera.edge(i).reverse();

    if (s == 1 && emitter[0].isInfiniteEmitter())
        edges[0] = PathEdge(emitter[0].emitterRecord().direction.d, 1.0f, 1.0f);
    else if (s != 0 && t != 0)
        edges[s - 1] = PathEdge(emitter[s - 1], camera[t - 1]);

    emitterSampler.seek(0);
    if (!emitter[0].invertRootVertex(emitterSampler, *vertices[0]))
        return false;
    for (int i = 0; i < newS - 1; ++i) {
        const PathVertex *v = (i == 0 ? &emitter[0] : vertices[i]);
        if (!v->invertVertex(emitterSampler, i ? &edges[i - 1] : nullptr, edges[i], *vertices[i + 1]))
            return false;
        emitterSampler.seek(i + 1);
    }

    cameraSampler.seek(0);
    if (!camera[0].invertRootVertex(cameraSampler, *vertices[vEnd]))
        return false;
    PathEdge prevEdge;
    for (int i = 0; i < newT - 1; ++i) {
        PathEdge nextEdge = edges[eEnd - i].reverse();
        const PathVertex *v = (i == 0 ? &camera[0] : vertices[vEnd - i]);
        if (!v->invertVertex(cameraSampler, i ? &prevEdge : nullptr, nextEdge, *vertices[vEnd - (i + 1)]))
            return false;
        prevEdge = nextEdge;
        cameraSampler.seek(i + 1);
    }

    return true;
}
}
