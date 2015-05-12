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

struct PathVertex
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

    VertexType type;
    VertexRecord _record;
    VertexSampler sampler;

    Vec3f throughput;
    float pdfForward;
    float pdfBackward;
    bool isConnectable;

    PathVertex() = default;
    PathVertex(const Primitive *emitter, float emitterPdf)
    : type(EmitterVertex),
      _record(EmitterRecord(emitterPdf)),
      sampler(emitter),
      isConnectable(true)
    {
    }
    PathVertex(const Camera *camera, Vec2u pixel)
    : type(CameraVertex),
      _record(CameraRecord(pixel)),
      sampler(camera),
      isConnectable(true)
    {
    }
    PathVertex(const Bsdf *bsdf, const SurfaceRecord &surface, const Vec3f &throughput_)
    : type(SurfaceVertex),
      _record(surface),
      sampler(bsdf),
      throughput(throughput_),
      isConnectable(!bsdf->lobes().isPureSpecular())
    {
    }
    PathVertex(const Medium *medium, const VolumeScatterEvent &event, const Vec3f &throughput_)
    : type(VolumeVertex),
      _record(event),
      sampler(medium),
      throughput(throughput_),
      isConnectable(true)
    {
    }

    bool sampleRootVertex(TraceState &state);
    bool sampleNextVertex(const TraceableScene &scene, TraceBase &tracer, TraceState &state,
            PathVertex *prev, PathEdge *prevEdge, PathVertex &next, PathEdge &nextEdge);

    Vec3f eval(const Vec3f &d) const;
    void evalPdfs(const PathVertex *prev, const PathEdge *prevEdge, const PathVertex &next,
            const PathEdge &nextEdge, float *forward, float *backward) const;

    Vec3f pos() const;

    float cosineFactor(const Vec3f &d) const;
};

}

#endif /* PATHVERTEX_HPP_ */
