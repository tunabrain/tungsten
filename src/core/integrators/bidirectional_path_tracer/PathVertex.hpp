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

struct PathVertex
{
    enum VertexType
    {
        EmitterRoot,
        CameraRoot,
        EmitterAreaVertex,
        EmitterDirectionVertex,
        CameraAreaVertex,
        CameraDirectionVertex,
        SurfaceVertex,
        VolumeVertex,
        InvalidVertex
    };

    struct SurfaceRecord
    {
        SurfaceScatterEvent event;
        IntersectionTemporary data;
        IntersectionInfo info;
    };

    union VertexRecord
    {
        PositionSample point;
        DirectionSample direction;
        SurfaceRecord surface;
        VolumeScatterEvent volume;

        VertexRecord() = default;
        VertexRecord(const      PositionSample &    point_) :     point(    point_) {}
        VertexRecord(const     DirectionSample &direction_) : direction(direction_) {}
        VertexRecord(const  VolumeScatterEvent &   volume_) :    volume(   volume_) {}
        VertexRecord(const SurfaceScatterEvent &surface_, const IntersectionTemporary &data_,
                const IntersectionInfo &info_)
        : surface(SurfaceRecord{surface_, data_, info_})
        {
            surface.event = surface_;
            surface.data = data_;
            surface.info = info_;
        }
    };
    union VertexSampler
    {
        const Camera *camera;
        const Primitive *primitive;
        const Bsdf *bsdf;
        const Medium *medium;

        VertexSampler() = default;
        VertexSampler(const    Camera *   camera_) :    camera(   camera_) {}
        VertexSampler(const Primitive *primitive_) : primitive(primitive_) {}
        VertexSampler(const      Bsdf *     bsdf_) :      bsdf(     bsdf_) {}
        VertexSampler(const    Medium *   medium_) :    medium(   medium_) {}
    };

    VertexType type;
    VertexRecord record;
    VertexSampler sampler;

    PathVertex() = default;
    PathVertex(const Camera *camera)
    : type(CameraRoot),
      sampler(camera)
    {
    }
    PathVertex(const Camera *camera, const PositionSample &point)
    : type(CameraAreaVertex),
      record(point),
      sampler(camera)
    {
    }
    PathVertex(const Camera *camera, const DirectionSample &direction)
    : type(CameraDirectionVertex),
      record(direction),
      sampler(camera)
    {
    }
    PathVertex(const Primitive *emitter)
    : type(EmitterRoot),
      sampler(emitter)
    {
    }
    PathVertex(const Primitive *emitter, const PositionSample &point)
    : type(EmitterAreaVertex),
      record(point),
      sampler(emitter)
    {
    }
    PathVertex(const Primitive *emitter, const DirectionSample &direction)
    : type(EmitterDirectionVertex),
      record(direction),
      sampler(emitter)
    {
    }
    PathVertex(const Bsdf *bsdf, const SurfaceScatterEvent &event, const IntersectionTemporary &data,
            const IntersectionInfo &info)
    : type(SurfaceVertex),
      record(event, data, info),
      sampler(bsdf)
    {
    }
    PathVertex(const Medium *medium, const VolumeScatterEvent &event)
    : type(VolumeVertex),
      record(event),
      sampler(medium)
    {
    }

    Vec3f weight() const
    {
        switch (type) {
        case EmitterRoot:
        case CameraRoot:
            return Vec3f(1.0f);
        case EmitterAreaVertex:
        case CameraAreaVertex:
            return record.point.weight;
        case EmitterDirectionVertex:
        case CameraDirectionVertex:
            return record.direction.weight;
        case SurfaceVertex:
            return record.surface.event.throughput;
        case VolumeVertex:
            return record.volume.throughput;
        default:
            return Vec3f(0.0f);
        }
    }

    float pdf() const
    {
        switch (type) {
        case EmitterRoot:
        case CameraRoot:
            return 1.0f;
        case EmitterAreaVertex:
        case CameraAreaVertex:
            return record.point.pdf;
        case EmitterDirectionVertex:
        case CameraDirectionVertex:
            return record.direction.pdf;
        case SurfaceVertex:
            return record.surface.event.pdf;
        case VolumeVertex:
            return record.volume.pdf;
        default:
            return 0.0f;
        }
    }

    bool scatter(const TraceableScene &scene, TraceBase &tracer, TraceState &state, PathVertex &dst)
    {
        dst.type = InvalidVertex;

        switch (type) {
        case EmitterRoot: {
            PositionSample point;
            if (!sampler.primitive->samplePosition(state.sampler, point))
                return false;
            dst = PathVertex(sampler.primitive, point);
            state.ray.setPos(point.p);
            state.ray.setPrimaryRay(true);
            return true;
        } case CameraRoot: { // TODO: Set wasSpecular depending on pinhole/thinlens camera
            PositionSample point;
            if (!sampler.camera->samplePosition(state.sampler, point))
                return false;
            dst = PathVertex(sampler.camera, point);
            state.ray.setPos(point.p);
            return true;
        } case EmitterAreaVertex: {
            DirectionSample direction;
            if (!sampler.primitive->sampleDirection(state.sampler, record.point, direction))
                return false;
            dst = PathVertex(sampler.primitive, direction);
            state.ray.setDir(direction.d);
            return true;
        } case CameraAreaVertex: {
            // TODO: Condition on pixel how/where?
            DirectionSample direction;
            if (!sampler.camera->sampleDirection(state.sampler, record.point, direction))
                return false;
            dst = PathVertex(sampler.camera, direction);
            state.ray.setDir(direction.d);
            return true;
        } case EmitterDirectionVertex:
        case CameraDirectionVertex:
        case SurfaceVertex: { // TODO: Participating media
            IntersectionTemporary data;
            IntersectionInfo info;
            bool didHit = scene.intersect(state.ray, data, info);
            if (!didHit)
                return false;

            SurfaceScatterEvent event = tracer.makeLocalScatterEvent(data, info, state.ray,
                    &state.sampler, &state.supplementalSampler);

            Vec3f throughput(1.0f);
            Vec3f emission(0.0f);
            bool scattered = tracer.handleSurface(event, data, info, state.sampler,
                    state.supplementalSampler, state.medium, state.bounce,
                    false, state.ray, throughput, emission, state.wasSpecular, state.mediumState);

            dst = PathVertex(info.bsdf, event, data, info);

            if (!scattered)
                return false;

            state.bounce++;
            return true;
        } case VolumeVertex:
            return false;
        default:
            return false;
        }
    }

    const Camera *camera() const
    {
        return sampler.camera;
    }

    const Primitive *emitter() const
    {
        return sampler.primitive;
    }

    const Bsdf *bsdf() const
    {
        return sampler.bsdf;
    }

    const Medium *medium() const
    {
        return sampler.medium;
    }

    const Primitive *surface() const
    {
        return record.surface.info.primitive;
    }

    const PositionSample &positionSample() const
    {
        return record.point;
    }

    const SurfaceScatterEvent &surfaceEvent() const
    {
        return record.surface.event;
    }

    bool valid() const
    {
        return type != InvalidVertex;
    }
};

}

#endif /* PATHVERTEX_HPP_ */
