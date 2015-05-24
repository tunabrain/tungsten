#ifndef PATHVERTEX_HPP_
#define PATHVERTEX_HPP_

#include "SurfaceRecord.hpp"
#include "EmitterRecord.hpp"
#include "CameraRecord.hpp"

#include "samplerecords/VolumeScatterEvent.hpp"

#include "bsdfs/Bsdf.hpp"

namespace Tungsten {

class TraceableScene;
class TraceState;
class TraceBase;
struct PathEdge;
class Camera;
class Medium;

class PathVertex
{
    enum VertexType
    {
        EmitterVertex,
        CameraVertex,
        SurfaceVertex,
        VolumeVertex
    };

    union VertexRecord
    {
        CameraRecord camera;
        EmitterRecord emitter;
        SurfaceRecord surface;
        VolumeScatterEvent volume;

        VertexRecord() = default;
        VertexRecord(const        CameraRecord &     camera_) :      camera(     camera_) {}
        VertexRecord(const       EmitterRecord &    emitter_) :     emitter(    emitter_) {}
        VertexRecord(const  VolumeScatterEvent &     volume_) :      volume(     volume_) {}
        VertexRecord(const       SurfaceRecord &    surface_) :     surface(    surface_) {}
    };
    union VertexSampler
    {
        const Camera *camera;
        const Primitive *emitter;
        const Bsdf *bsdf;
        const Medium *medium;

        VertexSampler() = default;
        VertexSampler(const    Camera * camera_) :  camera( camera_) {}
        VertexSampler(const Primitive *emitter_) : emitter(emitter_) {}
        VertexSampler(const      Bsdf *   bsdf_) :    bsdf(   bsdf_) {}
        VertexSampler(const    Medium * medium_) :  medium( medium_) {}
    };

    VertexType _type;
    VertexRecord _record;
    VertexSampler _sampler;

    Vec3f _throughput;
    float _pdfForward;
    float _pdfBackward;
    bool _connectable;

public:
    PathVertex() = default;
    PathVertex(const Primitive *emitter, float emitterPdf)
    : _type(EmitterVertex),
      _record(EmitterRecord(emitterPdf)),
      _sampler(emitter),
      _connectable(true)
    {
    }
    PathVertex(const Camera *camera, Vec2u pixel)
    : _type(CameraVertex),
      _record(CameraRecord(pixel)),
      _sampler(camera),
      _connectable(true)
    {
    }
    PathVertex(const Bsdf *bsdf, const SurfaceRecord &surface, const Vec3f &throughput_)
    : _type(SurfaceVertex),
      _record(surface),
      _sampler(bsdf),
      _throughput(throughput_),
      _connectable(!bsdf->lobes().isPureSpecular())
    {
    }
    PathVertex(const Medium *medium, const VolumeScatterEvent &event, const Vec3f &throughput_)
    : _type(VolumeVertex),
      _record(event),
      _sampler(medium),
      _throughput(throughput_),
      _connectable(true)
    {
    }

    bool sampleRootVertex(TraceState &state);
    bool sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state, bool adjoint,
            PathVertex *prev, PathEdge *prevEdge, PathVertex &next, PathEdge &nextEdge);

    Vec3f eval(const Vec3f &d, bool adjoint) const;
    void evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
            const PathEdge &nextEdge, float *forward, float *backward) const;

    Vec3f pos() const;

    float cosineFactor(const Vec3f &d) const;

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

    const VolumeScatterEvent &volumeRecord() const
    {
        return _record.volume;
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

    const Medium *medium() const
    {
        return _sampler.medium;
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

    bool connectable() const
    {
        return _connectable;
    }
};

}

#endif /* PATHVERTEX_HPP_ */
