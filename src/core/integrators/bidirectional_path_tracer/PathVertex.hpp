#ifndef PATHVERTEX_HPP_
#define PATHVERTEX_HPP_

#include "SurfaceRecord.hpp"
#include "EmitterRecord.hpp"
#include "CameraRecord.hpp"
#include "MediumRecord.hpp"

#include "primitives/Primitive.hpp"

#include "cameras/Camera.hpp"

#include "bsdfs/Bsdf.hpp"

namespace Tungsten {

class TraceableScene;
struct TraceState;
class TraceBase;
struct PathEdge;
class Medium;

class PathVertex
{
    enum VertexType
    {
        EmitterVertex,
        CameraVertex,
        SurfaceVertex,
        MediumVertex
    };

    union VertexRecord
    {
        CameraRecord camera;
        EmitterRecord emitter;
        SurfaceRecord surface;
        MediumRecord medium;

        VertexRecord() = default;
        VertexRecord(const  CameraRecord &  camera_) :  camera( camera_) {}
        VertexRecord(const EmitterRecord & emitter_) : emitter(emitter_) {}
        VertexRecord(const  MediumRecord &  medium_) :  medium( medium_) {}
        VertexRecord(const SurfaceRecord & surface_) : surface(surface_) {}
    };
    union VertexSampler
    {
        const Camera *camera;
        const Primitive *emitter;
        const Bsdf *bsdf;
        const PhaseFunction *phase;

        VertexSampler() = default;
        VertexSampler(const       Camera * camera_) :  camera( camera_) {}
        VertexSampler(const     Primitive *emitter_) : emitter(emitter_) {}
        VertexSampler(const          Bsdf *   bsdf_) :    bsdf(   bsdf_) {}
        VertexSampler(const PhaseFunction *  phase_) :   phase(  phase_) {}
    };

    VertexType _type;
    VertexRecord _record;
    VertexSampler _sampler;
    const Medium *_medium;

    Vec3f _throughput;
    float _pdfForward;
    float _pdfBackward;
    bool _dirac;
    bool _forward;
    bool _connectable;

public:
    PathVertex() = default;
    PathVertex(const Primitive *emitter, float emitterPdf)
    : _type(EmitterVertex),
      _record(EmitterRecord(emitterPdf)),
      _sampler(emitter),
      _medium(emitter->extMedium().get()),
      _dirac(false),
      _forward(false),
      _connectable(!_dirac)
    {
    }
    PathVertex(const Camera *camera, Vec2u pixel)
    : _type(CameraVertex),
      _record(CameraRecord(pixel)),
      _sampler(camera),
      _medium(camera->medium().get()),
      _dirac(camera->isFilterDirac()),
      _forward(false),
      _connectable(!_dirac)
    {
    }
    PathVertex(const Camera *camera)
    : _type(CameraVertex),
      _sampler(camera),
      _dirac(camera->isFilterDirac()),
      _connectable(!_dirac)
    {
        _record.camera.hasPixel = false;
    }
    PathVertex(const Bsdf *bsdf, const SurfaceRecord &surface, const Vec3f &throughput_)
    : _type(SurfaceVertex),
      _record(surface),
      _sampler(bsdf),
      _medium(nullptr),
      _throughput(throughput_),
      _dirac(false),
      _forward(false),
      _connectable(bsdf == nullptr || !bsdf->lobes().isPureDirac())
    {
    }
    PathVertex(const PhaseFunction *phase, const MediumRecord &medium, const Vec3f &throughput_)
    : _type(MediumVertex),
      _record(medium),
      _sampler(phase),
      _medium(nullptr),
      _throughput(throughput_),
      _dirac(false),
      _forward(false),
      _connectable(!_dirac)
    {
    }

    bool sampleRootVertex(TraceState &state);
    bool sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state, bool adjoint,
            PathVertex *prev, PathEdge *prevEdge, PathVertex &next, PathEdge &nextEdge);
    bool invertRootVertex(WritablePathSampleGenerator &sampler, const PathVertex &next) const;
    bool invertVertex(WritablePathSampleGenerator &sampler, const PathEdge *prevEdge,
            const PathEdge &nextEdge, const PathVertex &nextVert) const;

    Vec3f eval(const Vec3f &d, bool adjoint) const;
    void evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
            const PathEdge &nextEdge, float *forward, float *backward) const;
    bool segmentConnectable(const PathVertex &next) const;

    void pointerFixup();

    Vec3f pos() const;

    float cosineFactor(const Vec3f &d) const;
    const Medium *selectMedium(const Vec3f &d) const;

    const Medium *medium() const
    {
        return _medium;
    }

    const CameraRecord &cameraRecord() const
    {
        return _record.camera;
    }

    const EmitterRecord &emitterRecord() const
    {
        return _record.emitter;
    }

    const SurfaceRecord &surfaceRecord() const
    {
        return _record.surface;
    }

    const MediumRecord &mediumRecord() const
    {
        return _record.medium;
    }

    const Camera *camera() const
    {
        return _sampler.camera;
    }

    const Primitive *emitter() const
    {
        return _sampler.emitter;
    }

    const Bsdf *bsdf() const
    {
        return _sampler.bsdf;
    }

    const PhaseFunction *phase() const
    {
        return _sampler.phase;
    }

    const Vec3f &throughput() const
    {
        return _throughput;
    }

    float pdfForward() const
    {
        return _pdfForward;
    }

    float pdfBackward() const
    {
        return _pdfBackward;
    }

    float &pdfForward()
    {
        return _pdfForward;
    }

    float &pdfBackward()
    {
        return _pdfBackward;
    }

    bool connectable() const
    {
        return _connectable;
    }

    bool isDirac() const
    {
        return _dirac;
    }

    bool isForward() const
    {
        return _forward;
    }

    bool isInfiniteEmitter() const
    {
        return _type == EmitterVertex && _sampler.emitter->isInfinite();
    }

    bool isInfiniteSurface() const
    {
        return _type == SurfaceVertex && _record.surface.info.primitive->isInfinite();
    }

    bool onSurface() const
    {
        return _type != MediumVertex;
    }
};

}

#endif /* PATHVERTEX_HPP_ */
