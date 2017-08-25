#ifndef TRACEABLESCENE_HPP_
#define TRACEABLESCENE_HPP_

#include "integrators/Integrator.hpp"

#include "primitives/InfiniteSphere.hpp"
#include "primitives/EmbreeUtil.hpp"
#include "primitives/Primitive.hpp"

#include "textures/ConstantTexture.hpp"

#include "cameras/Camera.hpp"

#include "media/Medium.hpp"

#include "RendererSettings.hpp"
#include <vector>
#include <memory>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

namespace Tungsten {

class TraceableScene
{
public:
    struct IntersectionRay : RTCRay
    {
        IntersectionTemporary &data;
        Ray &ray;
        unsigned userGeomId;

        IntersectionRay(RTCRay eRay, IntersectionTemporary &data_, Ray &ray_, unsigned userGeomId_)
        : RTCRay(eRay), data(data_), ray(ray_), userGeomId(userGeomId_) {}
    };

private:
    const float DefaultEpsilon = 5e-4f;

    Camera &_cam;
    Integrator &_integrator;
    std::vector<std::shared_ptr<Primitive>> &_primitives;
    std::vector<std::shared_ptr<Bsdf>> &_bsdfs;
    std::vector<std::shared_ptr<Medium>> &_media;
    std::vector<std::shared_ptr<Primitive>> _lights;
    std::vector<std::shared_ptr<Primitive>> _infiniteLights;
    std::vector<const Primitive *> _finites;
    RendererSettings _settings;

    RTCScene _scene = nullptr;
    unsigned _userGeomId;

    Box3f _sceneBounds;

public:
    TraceableScene(Camera &cam, Integrator &integrator,
            std::vector<std::shared_ptr<Primitive>> &primitives,
            std::vector<std::shared_ptr<Bsdf>> &bsdfs,
            std::vector<std::shared_ptr<Medium>> &media,
            RendererSettings settings,
            uint32 seed)
    : _cam(cam),
      _integrator(integrator),
      _primitives(primitives),
      _bsdfs(bsdfs),
      _media(media),
      _settings(settings)
    {
        _cam.prepareForRender();
        _cam.requestOutputBuffers(_settings.renderOutputs());

        for (std::shared_ptr<Medium> &m : _media)
            m->prepareForRender();

        for (std::shared_ptr<Bsdf> &b : _bsdfs)
            b->prepareForRender();

        int finiteCount = 0, lightCount = 0;
        for (std::shared_ptr<Primitive> &m : _primitives) {
            m->prepareForRender();
            for (int i = 0; i < m->numBsdfs(); ++i)
                if (m->bsdf(i)->unnamed())
                    m->bsdf(i)->prepareForRender();

            if (!m->isDirac() && !m->isInfinite())
                finiteCount++;

            if (m->isEmissive()) {
                lightCount++;
                if (m->isSamplable())
                    _lights.push_back(m);
                if (m->isInfinite())
                    _infiniteLights.push_back(m);
            }
        }
        if (lightCount == 0) {
            std::shared_ptr<InfiniteSphere> defaultLight = std::make_shared<InfiniteSphere>();
            defaultLight->setEmission(std::make_shared<ConstantTexture>(1.0f));
            _lights.push_back(defaultLight);
            _infiniteLights.push_back(defaultLight);
        }

        for (std::shared_ptr<Primitive> &m : _primitives) {
            if (m->isInfinite() || m->isDirac())
                continue;

            _sceneBounds.grow(m->bounds());
            _finites.push_back(m.get());
        }

        if (_settings.useSceneBvh()) {
            _scene = rtcDeviceNewScene(EmbreeUtil::getDevice(), RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT, RTC_INTERSECT1);
            _userGeomId = rtcNewUserGeometry(_scene, _finites.size());
            rtcSetUserData(_scene, _userGeomId, this);

            rtcSetBoundsFunction(_scene, _userGeomId, [](void *ptr, size_t i, RTCBounds &bounds) {
                bounds = EmbreeUtil::convert(static_cast<TraceableScene *>(ptr)->finites()[i]->bounds());
            });
            rtcSetIntersectFunction(_scene, _userGeomId, [](void *ptr, RTCRay &embreeRay, size_t i) {
                IntersectionRay &ray = *static_cast<IntersectionRay *>(&embreeRay);
                if (static_cast<TraceableScene *>(ptr)->finites()[i]->intersect(ray.ray, ray.data)) {
                    embreeRay.tfar = ray.ray.farT();
                    embreeRay.geomID = ray.userGeomId;
                    embreeRay.primID = i;
                }
            });
            rtcSetOccludedFunction(_scene, _userGeomId, [](void *ptr, RTCRay &embreeRay, size_t i) {
                if (static_cast<TraceableScene *>(ptr)->finites()[i]->occluded(Ray(EmbreeUtil::convert(embreeRay))))
                    embreeRay.geomID = 0;
            });

            rtcCommit(_scene);
        }

        _integrator.prepareForRender(*this, seed);
    }

    ~TraceableScene()
    {
        _integrator.teardownAfterRender();
        _cam.teardownAfterRender();

        for (std::shared_ptr<Medium> &m : _media)
            m->teardownAfterRender();

        for (std::shared_ptr<Bsdf> &b : _bsdfs)
            b->teardownAfterRender();

        for (std::shared_ptr<Primitive> &m : _primitives) {
            m->teardownAfterRender();
            for (int i = 0; i < m->numBsdfs(); ++i)
                if (m->bsdf(i)->unnamed())
                    m->bsdf(i)->teardownAfterRender();
        }

        if (_scene)
            rtcDeleteScene(_scene);
        _scene = nullptr;
    }

    float hitDistance(Ray &ray) const
    {
        IntersectionTemporary data;
        IntersectionRay eRay(EmbreeUtil::convert(ray), data, ray, _userGeomId);
        rtcIntersect(_scene, eRay);
        return eRay.ray.farT();
    }

    bool intersect(Ray &ray, IntersectionTemporary &data, IntersectionInfo &info) const
    {
        info.primitive = nullptr;
        data.primitive = nullptr;

        if (_settings.useSceneBvh()) {
            IntersectionRay eRay(EmbreeUtil::convert(ray), data, ray, _userGeomId);
            rtcIntersect(_scene, eRay);
        } else {
            for (const Primitive *prim : _finites)
                prim->intersect(ray, data);
        }

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

    bool intersectInfinites(Ray &ray, IntersectionTemporary &data, IntersectionInfo &info) const
    {
        info.primitive = nullptr;
        data.primitive = nullptr;

        for (const std::shared_ptr<Primitive> &p : _infiniteLights)
            p->intersect(ray, data);

        if (data.primitive) {
            info.w = ray.dir();
            data.primitive->intersectionInfo(data, info);
            return true;
        } else {
            return false;
        }
    }

    bool occluded(const Ray &ray) const
    {
        if (_settings.useSceneBvh()) {
            auto eRay = EmbreeUtil::convert(ray);
            rtcOccluded(_scene, eRay);
            return eRay.geomID != RTC_INVALID_GEOMETRY_ID;
        } else {
            for (const Primitive *prim : _finites)
                if (prim->occluded(ray))
                    return true;
            return false;
        }
    }

    const Box3f &bounds() const
    {
        return _sceneBounds;
    }

    Camera &cam() const
    {
        return _cam;
    }

    Integrator &integrator() const
    {
        return _integrator;
    }

    const std::vector<std::shared_ptr<Primitive>> &primitives() const
    {
        return _primitives;
    }

    std::vector<std::shared_ptr<Primitive>> &lights()
    {
        return _lights;
    }

    const std::vector<std::shared_ptr<Primitive>> &lights() const
    {
        return _lights;
    }

    const std::vector<const Primitive *> &finites() const
    {
        return _finites;
    }

    const std::vector<std::shared_ptr<Medium>> &media() const
    {
        return _media;
    }

    RendererSettings rendererSettings() const
    {
        return _settings;
    }

    RTCScene scene() const
    {
        return _scene;
    }
};

}

#endif /* TRACEABLESCENE_HPP_ */
