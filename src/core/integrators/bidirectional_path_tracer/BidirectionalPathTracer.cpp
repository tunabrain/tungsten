#include "BidirectionalPathTracer.hpp"

#include <thread>

namespace Tungsten {

BidirectionalPathTracer::BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings,
        uint32 threadId)
: TraceBase(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _cameraPath(new PathVertex[settings.maxBounces + 4]),
  _lightPath(new PathVertex[settings.maxBounces + 4])
{
    std::vector<float> lightWeights(scene->lights().size());
    for (size_t i = 0; i < scene->lights().size(); ++i) {
        scene->lights()[i]->makeSamplable(_threadId);
        lightWeights[i] = 1.0f; // TODO
    }
    _lightSampler.reset(new Distribution1D(std::move(lightWeights)));
}

int BidirectionalPathTracer::traceCameraPath(SampleGenerator &sampler, UniformSampler &supplementalSampler, Vec2u pixel)
{
    const Camera *camera = &_scene->cam();

    PositionSample point;
    if (!camera->samplePosition(sampler, point))
        return 0;
    DirectionSample direction;
    if (!camera->sampleDirection(sampler, point, pixel, direction))
        return 0;

    int idx = 0;
    _cameraPath[idx++] = PathVertex(camera);
    _cameraPath[idx++] = PathVertex(camera, point);
    _cameraPath[idx++] = PathVertex(camera, direction);

    TraceState state(sampler, supplementalSampler);

    state.ray = Ray(point.p, direction.d);
    state.ray.setPrimaryRay(true);

    while (state.bounce < _settings.maxBounces) {
        if (!_cameraPath[idx - 1].scatter(*_scene, *this, state, _cameraPath[idx]))
            break;
        idx++;
    }

    if (_cameraPath[idx].valid())
        idx++;

    return idx;
}

int BidirectionalPathTracer::traceLightPath(SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    float u = supplementalSampler.next1D();
    int lightIdx;
    _lightSampler->warp(u, lightIdx);
    const Primitive *light = _scene->lights()[lightIdx].get(); // TODO: Multiple lights, for real

    int idx = 0;
    _lightPath[idx++] = PathVertex(light);

    TraceState state(sampler, supplementalSampler);

    while (state.bounce < _settings.maxBounces) {
        if (!_lightPath[idx - 1].scatter(*_scene, *this, state, _lightPath[idx]))
            break;
        idx++;
    }

    if (_lightPath[idx].valid())
        idx++;

    return idx;
}

Vec3f BidirectionalPathTracer::traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    int lightLength = traceLightPath(sampler, supplementalSampler);

    Vec3f throughput(1.0f);
    for (int i = 0; i < lightLength; ++i) {
        const PathVertex &vertex = _lightPath[i];

        if (vertex.type == PathVertex::SurfaceVertex) {
            SurfaceScatterEvent event = vertex.surfaceEvent();
            event.info = &vertex.record.surface.info; // TODO: Ugly pointer fixup - remove
            event.sampler = &sampler;
            event.supplementalSampler = &supplementalSampler;

            Vec3f splatWeight;
            Vec2u pixel;
            // TODO: Ray
            if (lensSample(_scene->cam(), event, nullptr, i - 3, Ray(), splatWeight, pixel))
                _splatBuffer->splat(pixel.x(), pixel.y(), Vec3f(splatWeight*throughput));
        } else if (vertex.type == PathVertex::EmitterAreaVertex) {
            PositionSample point = vertex.positionSample();

            LensSample splat;
            if (_scene->cam().sampleDirect(point.p, sampler, splat)) {
                Ray shadowRay(point.p, splat.d);
                shadowRay.setFarT(splat.dist);

                // TODO: Volume handling
                Vec3f transmission = generalizedShadowRay(shadowRay, nullptr, nullptr, 0);
                if (transmission != 0.0f) {
                    Vec3f value = throughput*transmission*splat.weight*point.weight
                            *vertex.emitter()->evalDirectionalEmission(point, DirectionSample(splat.d));
                    _splatBuffer->splat(splat.pixel.x(), splat.pixel.y(), value);
                }
            }
        }

        throughput *= vertex.weight();
    }

    return Vec3f(0.0f);

//    int idx = traceCameraPath(sampler, supplementalSampler, pixel);
//
//    Vec3f emission(0.0f);
//    Vec3f throughput(1.0f);
//    for (int i = 0; i < idx; ++i) {
//        const PathVertex &vertex = _cameraPath[i];
//
//        if (vertex.type == PathVertex::SurfaceVertex && vertex.surface()->isEmissive()) {
//            emission += throughput*vertex.surface()->evalDirect(vertex.record.surface.data,
//                    vertex.record.surface.info);
//        }
//        throughput *= vertex.weight();
//    }
//    return emission;
}

}
