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
    int numVerts = (s + 1) + (t + 1);
    float *pdfForward  = reinterpret_cast<float *>(alloca(numVerts*sizeof(float)));
    float *pdfBackward = reinterpret_cast<float *>(alloca(numVerts*sizeof(float)));

    for (int i = 0; i <= s; ++i) {
        pdfForward [i] = emitter[i].pdfForward;
        pdfBackward[i] = emitter[i].pdfBackward;
    }
    for (int i = 0; i <= t; ++i) {
        pdfForward [numVerts - 1 - i] = camera[i].pdfBackward;
        pdfBackward[numVerts - 1 - i] = camera[i].pdfForward;
    }

    PathEdge edge(emitter[s], camera[t]);
    emitter[s].evalPdfs(s == 0 ? nullptr : &emitter[s - 1],
                        s == 0 ? nullptr : &emitter.edge(s - 1),
                        camera[t], edge, &pdfForward[s + 1],
                        s == 0 ? nullptr : &pdfBackward[s - 1]);
    camera[t].evalPdfs(t == 0 ? nullptr : &camera[t - 1],
                       t == 0 ? nullptr : &camera.edge(t - 1),
                       emitter[s], edge.reverse(), &pdfBackward[s],
                       t == 0 ? nullptr : &pdfForward[s + 2]);

    float weight = 1.0f;
    float pi = 1.0f;
    for (int i = s; i < s + t; ++i) {
        pi *= pdfForward[i + 1]/pdfBackward[i + 1];
        weight += pi;
    }
    pi = 1.0f;
    for (int i = s - 1; i >= 1; --i) {
        pi *= pdfBackward[i + 1]/pdfForward[i + 1];
        weight += pi;
    }

    return 1.0f/weight;
}

}
