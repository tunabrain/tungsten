#include "PathVertex.hpp"
#include "PathEdge.hpp"

#include "integrators/TraceState.hpp"
#include "integrators/TraceBase.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/TraceableScene.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

namespace Tungsten {

bool PathVertex::sampleRootVertex(TraceState &state)
{
    switch (_type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;
        if (!_sampler.emitter->samplePosition(state.sampler, record.point))
            return false;

        _throughput = record.point.weight/record.emitterPdf;
        _pdfForward = record.point.pdf*record.emitterPdf;
        return true;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (!_sampler.camera->samplePosition(state.sampler, record.point))
            return false;

        _throughput = record.point.weight;
        _pdfForward = record.point.pdf;
        return true;
    } default:
        return false;
    }
}

bool PathVertex::sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state, bool adjoint,
        PathVertex *prev, PathEdge *prevEdge, PathVertex &next, PathEdge &nextEdge)
{
    Vec3f weight;
    float pdf;

    switch (_type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;
        if (!_sampler.emitter->sampleDirection(state.sampler, record.point, record.direction))
            return false;

        weight = record.direction.weight;
        pdf = record.direction.pdf;

        state.ray = Ray(record.point.p, record.direction.d);
        break;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (!_sampler.camera->sampleDirection(state.sampler, record.point, record.pixel, record.direction))
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
                state.medium, state.bounce, adjoint, false, state.ray,
                scatterWeight, emission, state.wasSpecular, state.mediumState);
        if (!scattered)
            return false;

        prev->_pdfBackward = _sampler.bsdf->pdf(record.event.makeFlippedQuery());
        if (prev->connectable())
            prev->_pdfBackward *= prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        //else
        //    prev->_pdfBackward /= cosineFactor(prevEdge->d);
        weight = record.event.throughput;
        pdf = record.event.pdf;

        break;
    } case VolumeVertex: {
        VolumeScatterEvent &record = _record.volume;

        // TODO: Participating media

        prev->_pdfBackward = _sampler.medium->phasePdf(_record.volume.makeFlippedQuery())
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

    record.event = tracer.makeLocalScatterEvent(record.data, record.info, state.ray, &state.sampler);

    next = PathVertex(record.info.bsdf, record, _throughput*weight);
    next._record.surface.event.info = &next._record.surface.info;
    state.bounce++;
    nextEdge = PathEdge(*this, next);
    next._pdfForward = pdf;
    if (next.connectable())
        next._pdfForward *= next.cosineFactor(nextEdge.d)/nextEdge.rSq;
    //else
    //    next._pdfForward /= cosineFactor(nextEdge.d);

    return true;
}

Vec3f PathVertex::eval(const Vec3f &d, bool adjoint) const
{
    switch (_type) {
    case EmitterVertex:
        return _sampler.emitter->evalDirectionalEmission(_record.emitter.point, DirectionSample(d));
    case CameraVertex:
        return Vec3f(0.0f);
    case SurfaceVertex:
        return _sampler.bsdf->eval(_record.surface.event.makeWarpedQuery(
                _record.surface.event.wi,
                _record.surface.event.frame.toLocal(d)),
                adjoint);
    case VolumeVertex:
        return _sampler.medium->phaseEval(_record.volume.makeWarpedQuery(_record.volume.wi, d));
    default:
        return Vec3f(0.0f);
    }
}

void PathVertex::evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
        const PathEdge &nextEdge, float *forward, float *backward) const
{
    switch (_type) {
    case EmitterVertex:
        *forward = next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                _sampler.emitter->directionalPdf(_record.emitter.point, DirectionSample(nextEdge.d));
        break;
    case CameraVertex:
        *forward = next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                _sampler.camera->directionPdf(_record.camera.point, DirectionSample(nextEdge.d));
        break;
    case SurfaceVertex: {
        const SurfaceScatterEvent &event = _record.surface.event;
        Vec3f dPrev = event.frame.toLocal(-prevEdge->d);
        Vec3f dNext = event.frame.toLocal(nextEdge.d);
        *forward  = next .cosineFactor(nextEdge .d)/nextEdge .rSq*
                _sampler.bsdf->pdf(event.makeWarpedQuery(dPrev, dNext));
        *backward = prev->cosineFactor(prevEdge->d)/prevEdge->rSq*
                _sampler.bsdf->pdf(event.makeWarpedQuery(dNext, dPrev));
        break;
    } case VolumeVertex: {
        const VolumeScatterEvent &event = _record.volume;
        Vec3f dPrev = -prevEdge->d;
        Vec3f dNext = nextEdge.d;
        *forward  = next .cosineFactor(nextEdge .d)/nextEdge .rSq*
                _sampler.medium->phasePdf(event.makeWarpedQuery(dPrev, dNext));
        *backward = prev->cosineFactor(prevEdge->d)/prevEdge->rSq*
                _sampler.medium->phasePdf(event.makeWarpedQuery(dNext, dPrev));
        break;
    }}
}

Vec3f PathVertex::pos() const
{
    switch (_type) {
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
    switch (_type) {
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

}
