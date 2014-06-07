#ifndef TRACEABLESCENE_HPP_
#define TRACEABLESCENE_HPP_

#include "integrators/Integrator.hpp"

#include "primitives/EmbreeUtil.hpp"
#include "primitives/Primitive.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

#include "RendererSettings.hpp"

#include <embree/include/intersector1.h>
#include <embree/geometry/virtual_scene.h>
#include <embree/common/ray.h>
#include <vector>
#include <memory>

namespace Tungsten {

class TraceableScene
{
    static constexpr float DefaultEpsilon = 5e-4f;

    struct PerRayData
    {
        Primitive::IntersectionTemporary &data;
        Ray &ray;
    };

    static void intersect(const void *userData, embree::Ray &eRay)
    {
        const Primitive *primitive = reinterpret_cast<const Primitive *>(userData);
        PerRayData *data = reinterpret_cast<PerRayData *>(eRay.userData);
//      if (primitive->needsRayTransform()) {
//          Ray ray(fromERay(eRay));
//          float length = ray.dir().length();
//          ray.setDir(ray.dir()*(1.0f/length));
//          ray.setNearT(ray.nearT()/length);
//          ray.setFarT(ray.farT()/length);
//          if (primitive->intersect(ray, data->data)) {
//              data->ray.setFarT(ray.farT()*length);
//              eRay.tfar = data->ray.farT();
//          }
//      } else {
            if (primitive->intersect(data->ray, data->data))
                eRay.tfar = data->ray.farT();
//      }
    }

    static bool occluded(const void *userData, embree::Ray &eRay)
    {
        const Primitive *primitive = reinterpret_cast<const Primitive *>(userData);
        return primitive->occluded(fromERay(eRay));
    }

    Camera &_cam;
    const Integrator &_integratorBase;
    std::vector<std::shared_ptr<Primitive>> &_primitives;
    std::vector<std::shared_ptr<Medium>> &_media;
    std::vector<std::shared_ptr<Primitive>> _lights;
    std::vector<std::shared_ptr<Primitive>> _infinites;
    RendererSettings _settings;

    embree::VirtualScene *_scene = nullptr;
    embree::Intersector1 *_intersector = nullptr;
    embree::Intersector1 _virtualIntersector;

public:
    TraceableScene(TraceableScene &&o) = default;

    TraceableScene(Camera &cam, const Integrator &integratorBase,
            std::vector<std::shared_ptr<Primitive>> &primitives,
            std::vector<std::shared_ptr<Medium>> &media,
            RendererSettings settings)
    : _cam(cam),
      _integratorBase(integratorBase),
      _primitives(primitives),
      _media(media),
      _settings(settings)
    {
        _virtualIntersector.intersectPtr = &intersect;
        _virtualIntersector.occludedPtr = &occluded;

        _cam.prepareForRender();

        for (std::shared_ptr<Medium> &m : _media)
            m->prepareForRender();

        int finiteCount = 0;
        for (std::shared_ptr<Primitive> &m : _primitives) {
            m->prepareForRender();

            if (m->isInfinite()) {
                _infinites.push_back(m);
            } else if (!m->isDelta()) {
                finiteCount++;
            }

            if (m->isEmissive()) {
                m->makeSamplable();
                if (m->isSamplable())
                    _lights.push_back(m);
            }
        }

        _scene = new embree::VirtualScene(finiteCount, "bvh2");
        embree::VirtualScene::Object *objects = _scene->objects;
        for (std::shared_ptr<Primitive> &m : _primitives) {
            if (m->isInfinite() || m->isDelta())
                continue;

            if (m->needsRayTransform()) {
                objects->hasTransform = true;
                objects->localBounds = toEBox(m->bounds());
                objects->local2world = toEMat(m->transform());
                objects->calculateWorldData();
            } else {
                objects->hasTransform = false;
                objects->localBounds = objects->worldBounds = toEBox(m->bounds());
            }

            /* TODO: Transforms */
            objects->userData = m.get();
            objects->intersector1 = &_virtualIntersector;
            objects++;
        }

        embree::rtcBuildAccel(_scene, "objectsplit");
        _intersector = embree::rtcQueryIntersector1(_scene, "fast");
    }

    ~TraceableScene()
    {
        _cam.teardownAfterRender();

        for (std::shared_ptr<Medium> &m : _media)
            m->cleanupAfterRender();

        for (std::shared_ptr<Primitive> &m : _primitives)
            m->cleanupAfterRender();

        embree::rtcDeleteGeometry(_scene);
        _scene = nullptr;
        _intersector = nullptr;
    }

    Integrator *cloneThreadSafeIntegrator(uint32 threadId) const
    {
        return _integratorBase.cloneThreadSafe(threadId, this);
    }

    bool intersect(Ray &ray, Primitive::IntersectionTemporary &data, IntersectionInfo &info) const
    {
        info.primitive = nullptr;
        data.primitive = nullptr;

        PerRayData rayData{data, ray};
        embree::Ray eRay(toERay(ray));
        eRay.userData = &rayData;

        _intersector->intersect(eRay);

        if (data.primitive) {
            info.p = ray.pos() + ray.dir()*ray.farT();
            info.w = ray.dir();
//          if (data.primitive->needsRayTransform()) {
//              Vec3f scale = data.primitive->transform().extractScaleVec();
//              float diagScale = scale.avg();
//              info.epsilon = DefaultEpsilon/diagScale;
//              data.primitive->intersectionInfo(data, info);
//              info.epsilon *= diagScale;
//              info.Ng = data.primitive->transform()
//          } else {
                info.epsilon = DefaultEpsilon;
                data.primitive->intersectionInfo(data, info);
//          }
            return true;
        } else {
            return false;
        }
    }

    bool intersectInfinites(Ray &ray, Primitive::IntersectionTemporary &data, IntersectionInfo &info) const
    {
        info.primitive = nullptr;
        data.primitive = nullptr;

        for (const std::shared_ptr<Primitive> &p : _infinites)
            p->intersect(ray, data);

        if (data.primitive) {
            info.p = ray.pos() + ray.dir()*ray.farT();
            info.w = ray.dir();
            info.epsilon = DefaultEpsilon;
            data.primitive->intersectionInfo(data, info);
            return true;
        } else {
            return false;
        }
    }

    bool occluded(const Ray &ray) const
    {
        embree::Ray eRay(toERay(ray));
        return _intersector->occluded(eRay);
    }

    Camera &cam() const
    {
        return _cam;
    }

    const std::vector<std::shared_ptr<Primitive>> &primitives() const
    {
        return _primitives;
    }

    const std::vector<std::shared_ptr<Primitive>> &lights() const
    {
        return _lights;
    }

    RendererSettings rendererSettings() const
    {
        return _settings;
    }
};

}

#endif /* TRACEABLESCENE_HPP_ */
