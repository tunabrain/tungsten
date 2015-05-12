#ifndef PATHVERTEX_HPP_
#define PATHVERTEX_HPP_

#include "samplerecords/SurfaceScatterEvent.hpp"
#include "samplerecords/VolumeScatterEvent.hpp"
#include "samplerecords/DirectionSample.hpp"
#include "samplerecords/PositionSample.hpp"

#include "integrators/TraceBase.hpp"

#include "primitives/IntersectionTemporary.hpp"
#include "primitives/IntersectionInfo.hpp"
#include "primitives/Primitive.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

#include "math/Ray.hpp"

namespace Tungsten {

class Medium;
class Bsdf;

struct TraceState
{
    SampleGenerator &sampler;
    UniformSampler &supplementalSampler;
    Medium::MediumState mediumState;
    const Medium *medium;
    bool wasSpecular;
    Ray ray;
    int bounce;

    TraceState(SampleGenerator &sampler_, UniformSampler &supplementalSampler_)
    : sampler(sampler_),
      supplementalSampler(supplementalSampler_),
      medium(nullptr),
      wasSpecular(true),
      ray(Vec3f(0.0f), Vec3f(0.0f)),
      bounce(0)
    {
        mediumState.reset();
    }
};

struct CameraRecord
{
    Vec2u pixel;
    PositionSample point;
    DirectionSample direction;

    CameraRecord() = default;
    CameraRecord(Vec2u pixel_)
    : pixel(pixel_)
    {
    }
};
struct EmitterRecord
{
    float emitterPdf;
    PositionSample point;
    DirectionSample direction;

    EmitterRecord() = default;
    EmitterRecord(float emitterPdf_)
    : emitterPdf(emitterPdf_)
    {
    }
};
struct SurfaceRecord
{
    SurfaceScatterEvent event;
    IntersectionTemporary data;
    IntersectionInfo info;
};

struct PathEdge;

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

struct PathEdge
{
    Vec3f d;
    float r;
    float rSq;

    PathEdge() = default;
    PathEdge(const Vec3f &d_, float r_, float rSq_)
    : d(d_),
      r(r_),
      rSq(rSq_)
    {
    }
    PathEdge(const PathVertex &root, const PathVertex &tip)
    {
        d = tip.pos() - root.pos();
        rSq = d.lengthSq();
        r = std::sqrt(rSq);
        if (r != 0.0f)
            d *= 1.0f/r;
    }

    PathEdge reverse() const
    {
        return PathEdge(-d, r, rSq);
    }
};

class LightPath
{
    int _maxLength;
    int _length;
    std::unique_ptr<PathVertex[]> _vertices;
    std::unique_ptr<PathEdge[]> _edges;

public:
    LightPath(int maxLength)
    : _maxLength(maxLength),
      _length(0),
      _vertices(new PathVertex[maxLength + 4]),
      _edges(new PathEdge[maxLength + 4])
    {
    }

    void startCameraPath(const Camera *camera, Vec2u pixel)
    {
        _vertices[0] = PathVertex(camera, pixel);
    }

    void startEmitterPath(const Primitive *emitter, float emitterPdf)
    {
        _vertices[0] = PathVertex(emitter, emitterPdf);
    }

    void tracePath(const TraceableScene &scene, TraceBase &tracer, SampleGenerator &sampler,
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

    int length() const
    {
        return _length;
    }

    PathVertex &operator[](int i)
    {
        return _vertices[i];
    }

    const PathVertex &operator[](int i) const
    {
        return _vertices[i];
    }

    PathEdge &edge(int i)
    {
        return _edges[i];
    }

    const PathEdge &edge(int i) const
    {
        return _edges[i];
    }

    Vec3f weightedPathEmission(int minLength, int maxLength) const;

    static Vec3f connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b);
    static bool connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b,
            SampleGenerator &sampler, Vec3f &weight, Vec2u &pixel);
    static float misWeight(const LightPath &camera, const LightPath &light, int s, int t);
};

}

#endif /* PATHVERTEX_HPP_ */
