#include "PathVertex.hpp"
#include "PathEdge.hpp"

#include "integrators/TraceState.hpp"
#include "integrators/TraceBase.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/TraceableScene.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "cameras/Camera.hpp"

#include "media/Medium.hpp"

namespace Tungsten {

bool PathVertex::sampleRootVertex(TraceState &state)
{
    switch (_type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;
        if (!_sampler.emitter->samplePosition(state.sampler, record.point))
            return false;

        // Infinite light sources are slightly awkward, because they sample
        // a direction before sampling a position. The sampling interfaces
        // don't directly allow for this, so the samplePosition samples
        // both direction and position, and we use sampleDirection to retrieve
        // the sampled direction. The pdfs/weights are then set appropriately
        // (direction pdf for the first vertex, position pdf for the next vertex)
        // Note that directional pdfs for infinite area lights are always in
        // solid angle measure.
        if (_sampler.emitter->isInfinite()) {
            if (!_sampler.emitter->sampleDirection(state.sampler, record.point, record.direction))
                return false;

            _throughput = record.direction.weight/record.emitterPdf;
            _pdfForward = record.direction.pdf*record.emitterPdf;
        } else {
            _throughput = record.point.weight/record.emitterPdf;
            _pdfForward = record.point.pdf*record.emitterPdf;
        }
        _medium = state.medium = _sampler.emitter->extMedium().get();
        return true;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (!_sampler.camera->samplePosition(state.sampler, record.point))
            return false;

        _throughput = record.point.weight;
        _pdfForward = record.point.pdf;
        _medium = state.medium = _sampler.camera->medium().get();
        return true;
    } default:
        return false;
    }
}

bool PathVertex::sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state, bool adjoint,
        PathVertex *prev, PathEdge */*prevEdge*/, PathVertex &next, PathEdge &nextEdge)
{
    Vec3f weight;
    float pdf;

    switch (_type) {
    case EmitterVertex: {
        EmitterRecord &record = _record.emitter;

        if (_sampler.emitter->isInfinite()) {
            weight = record.point.weight;
            pdf = record.point.pdf;
        } else {
            if (!_sampler.emitter->sampleDirection(state.sampler, record.point, record.direction))
                return false;

            weight = record.direction.weight;
            pdf = record.direction.pdf;
        }

        state.ray = Ray(record.point.p, record.direction.d);
        break;
    } case CameraVertex: {
        CameraRecord &record = _record.camera;
        if (record.hasPixel) {
            if (!_sampler.camera->sampleDirection(state.sampler, record.point, record.pixel, record.direction))
                return false;
        } else {
            if (!_sampler.camera->sampleDirectionAndPixel(state.sampler, record.point, record.pixel, record.direction))
                return false;
        }

        weight = record.direction.weight;
        pdf = record.direction.pdf;

        state.ray = Ray(record.point.p, record.direction.d);
        state.ray.setPrimaryRay(true);
        break;
    } case SurfaceVertex: {
        SurfaceRecord &record = _record.surface;

        if (record.info.primitive->isInfinite())
            return false;

        Vec3f scatterWeight(1.0f);
        Vec3f emission(0.0f);
        bool scattered = tracer.handleSurface(record.event, record.data, record.info,
                state.medium, state.bounce, adjoint, false, state.ray,
                scatterWeight, emission, state.wasSpecular, state.mediumState);
        if (!scattered)
            return false;

        if (record.event.sampledLobe.isForward())
            prev->_pdfBackward = record.event.pdf;
        else
            prev->_pdfBackward = _sampler.bsdf->pdf(record.event.makeFlippedQuery());
        _dirac = record.event.sampledLobe.isPureSpecular();
        _forward = record.event.sampledLobe.isForward();
        // Technically, we could connect to these kinds of vertices (e.g. BSDF with transparency),
        // but this creates so much headache for back-propagating the PDFs that we're just not
        // going to bother
        if (_forward)
            _connectable = false;
        weight = record.event.weight;
        pdf = record.event.pdf;

        break;
    } case MediumVertex: {
        MediumRecord &record = _record.medium;

        if (!record.mediumSample.phase->sample(state.sampler, state.ray.dir(), record.phaseSample))
            return false;

        prev->_pdfBackward = record.mediumSample.phase->pdf(-record.phaseSample.w, -state.ray.dir());

        state.ray = state.ray.scatter(record.mediumSample.p, record.phaseSample.w, 0.0f);
        state.ray.setPrimaryRay(false);

        weight = record.phaseSample.weight;
        pdf = record.phaseSample.pdf;

        break;
    } default:
        return false;
    }

    SurfaceRecord surfaceRecord;
    bool didHit = scene.intersect(state.ray, surfaceRecord.data, surfaceRecord.info);

    bool hitSurface;
    float edgePdfForward;
    float edgePdfBackward;
    MediumRecord mediumRecord;
    if (state.medium) {
        if (!state.medium->sampleDistance(state.sampler, state.ray, state.mediumState, mediumRecord.mediumSample))
            return false;
        if (mediumRecord.mediumSample.t < 1e-6f)
            return false;
        hitSurface = mediumRecord.mediumSample.exited;
        edgePdfForward = mediumRecord.mediumSample.pdf;
        Ray reverseRay(mediumRecord.mediumSample.p, -state.ray.dir(), 0.0f, mediumRecord.mediumSample.t);
        edgePdfBackward = state.medium->pdf(state.sampler, reverseRay, hitSurface, onSurface());
        weight *= mediumRecord.mediumSample.weight;
        if (hitSurface && !didHit)
            return false;
    } else {
        hitSurface = true;
        edgePdfForward = 1.0f;
        edgePdfBackward = 1.0f;
    }

    if (!hitSurface) {
        mediumRecord.wi = state.ray.dir();
        next = PathVertex(mediumRecord.mediumSample.phase, mediumRecord, _throughput*weight);
        next._medium = state.medium;
        state.bounce++;
        nextEdge = PathEdge(*this, next, edgePdfForward, edgePdfBackward);
        next._pdfForward = pdf;

        return true;
    } else if (didHit) {
        surfaceRecord.event = tracer.makeLocalScatterEvent(surfaceRecord.data, surfaceRecord.info, state.ray, &state.sampler);

        next = PathVertex(surfaceRecord.info.bsdf, surfaceRecord, _throughput*weight);
        next._medium = state.medium;
        next.pointerFixup();
        state.bounce++;
        nextEdge = PathEdge(*this, next, edgePdfForward, edgePdfBackward);
        next._pdfForward = pdf;

        return true;
    } else if (!adjoint && scene.intersectInfinites(state.ray, surfaceRecord.data, surfaceRecord.info)) {
        next = PathVertex(surfaceRecord.info.bsdf, surfaceRecord, _throughput*weight);
        next._medium = state.medium;
        state.bounce++;
        nextEdge = PathEdge(state.ray.dir(), 1.0f, 1.0f, edgePdfForward, edgePdfBackward);
        next._pdfForward = pdf;

        return true;
    } else {
        return false;
    }
}

bool PathVertex::invertRootVertex(WritablePathSampleGenerator &sampler, const PathVertex &next) const
{
    PositionSample point;
    switch (next._type) {
    case EmitterVertex:
        point = next._record.emitter.point;
        break;
    case CameraVertex:
        point = next._record.camera.point;
        break;
    case SurfaceVertex:
        point = PositionSample(next._record.surface.info);
        break;
    default:
        return false;
    }

    switch (_type) {
    case EmitterVertex:
        if (_sampler.emitter->isInfinite())
            return _sampler.emitter->invertDirection(sampler, point, DirectionSample(point.Ng));
        else
            return _sampler.emitter->invertPosition(sampler, point);
    case CameraVertex:
        return _sampler.camera->invertPosition(sampler, point);
    default:
        return false;
    }
}

bool PathVertex::invertVertex(WritablePathSampleGenerator &sampler, const PathEdge *prevEdge,
        const PathEdge &nextEdge, const PathVertex &nextVert) const
{
    if (selectMedium(nextEdge.d))
        selectMedium(nextEdge.d)->invertDistance(sampler, Ray(pos(), nextEdge.d, 0.0f, nextEdge.r), nextVert.onSurface());

    switch (_type) {
    case EmitterVertex: {
        if (_sampler.emitter->isInfinite())
            return _sampler.emitter->invertPosition(sampler, PositionSample(nextVert.pos(), nextEdge.d));
        else
            return _sampler.emitter->invertDirection(sampler, PositionSample(), DirectionSample(nextEdge.d));
    } case CameraVertex: {
        return _sampler.camera->invertDirection(sampler, _record.camera.point, DirectionSample(nextEdge.d));
    } case SurfaceVertex: {
        if (isInfiniteSurface())
            return false;

        Vec3f transparency = _sampler.bsdf->eval(_record.surface.event.makeForwardEvent(), false);
        float transparencyScalar = transparency.avg();
        sampler.putBoolean(transparencyScalar, isForward());

        if (isForward()) {
            return true;
        } else {
            Vec3f wi = _record.surface.event.frame.toLocal(-prevEdge->d);
            Vec3f wo = _record.surface.event.frame.toLocal( nextEdge. d);
            return _sampler.bsdf->invert(sampler, _record.surface.event.makeWarpedQuery(wi, wo));
        }
    } case MediumVertex: {
        return _sampler.phase->invert(sampler, prevEdge->d, nextEdge.d);
    } default:
        return false;
    }
}


Vec3f PathVertex::eval(const Vec3f &d, bool adjoint) const
{
    switch (_type) {
    case EmitterVertex:
        if (_sampler.emitter->isInfinite())
            return _sampler.emitter->evalPositionalEmission(_record.emitter.point);
        else
            return _sampler.emitter->evalDirectionalEmission(_record.emitter.point, DirectionSample(d));
    case CameraVertex:
        return Vec3f(0.0f);
    case SurfaceVertex:
        return _sampler.bsdf->eval(_record.surface.event.makeWarpedQuery(
                _record.surface.event.wi,
                _record.surface.event.frame.toLocal(d)),
                adjoint);
    case MediumVertex:
        return _sampler.phase->eval(_record.medium.wi, d);
    default:
        return Vec3f(0.0f);
    }
}

void PathVertex::evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
        const PathEdge &nextEdge, float *forward, float *backward) const
{
    switch (_type) {
    case EmitterVertex:
        if (_sampler.emitter->isInfinite())
            // Positional pdf is constant for a fixed direction, which is the case
            // for connections to a point on an infinite emitter
            *forward = nextEdge.pdfForward*_record.emitter.point.pdf*next.cosineFactor(nextEdge.d);
        else
            *forward = nextEdge.pdfForward*next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                    _sampler.emitter->directionalPdf(_record.emitter.point, DirectionSample(nextEdge.d));
        break;
    case CameraVertex:
        *forward = nextEdge.pdfForward*next.cosineFactor(nextEdge.d)/nextEdge.rSq*
                _sampler.camera->directionPdf(_record.camera.point, DirectionSample(nextEdge.d));
        break;
    case SurfaceVertex: {
        const SurfaceScatterEvent &event = _record.surface.event;
        Vec3f dPrev = event.frame.toLocal(-prevEdge->d);
        Vec3f dNext = event.frame.toLocal(nextEdge.d);
        *forward  = nextEdge .pdfForward *_sampler.bsdf->pdf(event.makeWarpedQuery(dPrev, dNext));
        *backward = prevEdge->pdfBackward*_sampler.bsdf->pdf(event.makeWarpedQuery(dNext, dPrev));
        if (!next .isInfiniteEmitter()) *forward  *= next .cosineFactor(nextEdge .d)/nextEdge .rSq;
        if (!prev->isInfiniteEmitter()) *backward *= prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        break;
    } case MediumVertex: {
        *forward  = nextEdge .pdfForward *_sampler.phase->pdf(prevEdge->d,   nextEdge.d);
        *backward = prevEdge->pdfBackward*_sampler.phase->pdf(-nextEdge.d, -prevEdge->d);
        if (!next .isInfiniteEmitter()) *forward  *= next .cosineFactor(nextEdge .d)/nextEdge .rSq;
        if (!prev->isInfiniteEmitter()) *backward *= prev->cosineFactor(prevEdge->d)/prevEdge->rSq;
        break;
    }}
}

bool PathVertex::segmentConnectable(const PathVertex &next) const
{
    if (onSurface() || next.onSurface())
        return true;

    return !_medium->isDirac();
}

void PathVertex::pointerFixup()
{
    // Yuck. It's best not to ask.
    // A combination of MSVC limitations and poor design decisions
    // have lead to this ugliness. Once the BSDF interface gets refactored
    // we can hopefully get rid of this.
    _record.surface.event.info = &_record.surface.info;
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
    case MediumVertex:
        return _record.medium.mediumSample.p;
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
    case MediumVertex:
        return 1.0f;
    default:
        return 0.0f;
    }
}

const Medium *PathVertex::selectMedium(const Vec3f &d) const
{
    switch (_type) {
    case EmitterVertex:
        return _sampler.emitter->extMedium().get();
    case CameraVertex:
        return _sampler.camera->medium().get();
    case SurfaceVertex: {
        const Primitive *p = _record.surface.info.primitive;
        return p->overridesMedia() ? p->selectMedium(_medium, d.dot(_record.surface.info.Ng) < 0.0f) : _medium;
    } case MediumVertex:
        return _medium;
    default:
        return nullptr;
    }
}

}
