#include "PathVertex.hpp"

namespace Tungsten {

bool PathVertex::sampleRootVertex(TraceState &state)
{
    switch (type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;
        if (!sampler.emitter->samplePosition(state.sampler, record.point))
            return false;

        throughput = record.point.weight/record.emitterPdf;
        pdfForward = record.point.pdf*record.emitterPdf;
        return true;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (!sampler.camera->samplePosition(state.sampler, record.point))
            return false;

        throughput = record.point.weight;
        pdfForward = record.point.pdf;
        return true;
    } default:
        return false;
    }
}

bool PathVertex::sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state,
        PathVertex *prev, PathEdge *prevEdge, PathVertex &next, PathEdge &nextEdge)
{
    Vec3f weight;
    float pdf;

    switch (type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;
        if (!sampler.emitter->sampleDirection(state.sampler, record.point, record.direction))
            return false;

        weight = record.direction.weight;
        pdf = record.direction.pdf;

        state.ray = Ray(record.point.p, record.direction.d);
        break;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (!sampler.camera->sampleDirection(state.sampler, record.point, record.pixel, record.direction))
            return false;

        weight = record.direction.weight;
        pdf = record.direction.pdf;

        state.ray = Ray(record.point.p, record.direction.d);
        state.ray.setPrimaryRay(true);
        break;
    } case SurfaceVertex: {
        SurfaceRecord &record = _record.surface;

        Vec3f scatterWeight(1.0f);
        Vec3f emission(0.0f);
        bool scattered = tracer.handleSurface(record.event, record.data, record.info, state.sampler,
                state.supplementalSampler, state.medium, state.bounce, false, state.ray,
                scatterWeight, emission, state.wasSpecular, state.mediumState);
        if (!scattered)
            return false;

        prev->pdfBackward = sampler.bsdf->pdf(record.event.makeFlippedQuery());
        if (prev->isConnectable)
            prev->pdfBackward *= prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        //else
        //    prev->pdfBackward /= cosineFactor(prevEdge->d);
        weight = record.event.throughput;
        pdf = record.event.pdf;

        break;
    } case VolumeVertex: {
        VolumeScatterEvent &record = _record.volume;

        // TODO: Participating media

        prev->pdfBackward = sampler.medium->phasePdf(_record.volume.makeFlippedQuery())
                *prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        weight = record.throughput;
        pdf = record.pdf;
        return false;
    } default:
        return false;
    }

    SurfaceRecord record;
    bool didHit = scene.intersect(state.ray, record.data, record.info);
    if (!didHit)
        return false;

    record.event = tracer.makeLocalScatterEvent(record.data, record.info, state.ray,
            &state.sampler, &state.supplementalSampler);

    next = PathVertex(record.info.bsdf, record, throughput*weight);
    next._record.surface.event.info = &next._record.surface.info;
    state.bounce++;
    nextEdge = PathEdge(*this, next);
    next.pdfForward = pdf;
    if (next.isConnectable)
        next.pdfForward *= next.cosineFactor(nextEdge.d)/nextEdge.rSq;
    //else
    //    next.pdfForward /= cosineFactor(nextEdge.d);

    return true;
}

Vec3f PathVertex::eval(const Vec3f &d) const
{
    switch (type) {
    case EmitterVertex:
        return sampler.emitter->evalDirectionalEmission(_record.emitter.point, DirectionSample(d));
    case CameraVertex:
        return Vec3f(0.0f);
    case SurfaceVertex:
        return sampler.bsdf->eval(_record.surface.event.makeWarpedQuery(
                _record.surface.event.wi,
                _record.surface.event.frame.toLocal(d)));
    case VolumeVertex:
        return sampler.medium->phaseEval(_record.volume.makeWarpedQuery(_record.volume.wi, d));
    default:
        return Vec3f(0.0f);
    }
}

void PathVertex::evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
        const PathEdge &nextEdge, float *forward, float *backward) const
{
    switch (type) {
    case EmitterVertex:
        *forward = next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                sampler.emitter->directionalPdf(_record.emitter.point, DirectionSample(nextEdge.d));
        break;
    case CameraVertex:
        *forward = next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                sampler.camera->directionPdf(_record.camera.point, DirectionSample(nextEdge.d));
        break;
    case SurfaceVertex: {
        const SurfaceScatterEvent &event = _record.surface.event;
        Vec3f dPrev = event.frame.toLocal(-prevEdge->d);
        Vec3f dNext = event.frame.toLocal(nextEdge.d);
        *forward  = sampler.bsdf->pdf(event.makeWarpedQuery(dPrev, dNext))*next .cosineFactor(nextEdge .d)/nextEdge .rSq;
        *backward = sampler.bsdf->pdf(event.makeWarpedQuery(dNext, dPrev))*prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        break;
    } case VolumeVertex: {
        const VolumeScatterEvent &event = _record.volume;
        Vec3f dPrev = -prevEdge->d;
        Vec3f dNext = nextEdge.d;
        *forward  = sampler.medium->phasePdf(event.makeWarpedQuery(dPrev, dNext))*next .cosineFactor(nextEdge .d)/nextEdge .rSq;
        *backward = sampler.medium->phasePdf(event.makeWarpedQuery(dNext, dPrev))*prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        break;
    }}
}

Vec3f PathVertex::pos() const
{
    switch (type) {
    case EmitterVertex:
        return _record.emitter.point.p;
    case CameraVertex:
        return _record.camera.point.p;
    case SurfaceVertex:
        return _record.surface.info.p;
    case VolumeVertex:
        return _record.volume.p;
    default:
        return Vec3f(0.0f);
    }
}

float PathVertex::cosineFactor(const Vec3f &d) const
{
    switch (type) {
    case EmitterVertex:
        return std::abs(_record.emitter.point.Ng.dot(d));
    case CameraVertex:
        return std::abs(_record.camera.point.Ng.dot(d));
    case SurfaceVertex:
        return std::abs(_record.surface.info.Ng.dot(d));
    case VolumeVertex:
        return 1.0f;
    default:
        return 0.0f;
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
            if (isConnectable[i - 1] && isConnectable[i]) {
                weight += pi;
            }
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
