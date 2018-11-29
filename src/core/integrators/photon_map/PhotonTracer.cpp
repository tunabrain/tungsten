#include "PhotonTracer.hpp"
#include "GridAccel.hpp"

#include "math/FastMath.hpp"

#include "bvh/BinaryBvh.hpp"

#include "Timer.hpp"

namespace Tungsten {

PhotonTracer::PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _settings(settings),
  _mailIdx(0),
  _photonQuery(new const Photon *[settings.gatherCount]),
  _distanceQuery(new float[settings.gatherCount]),
  _mailboxes(zeroAlloc<uint32>(settings.volumePhotonCount)),
  _indirectCache(settings.volumePhotonCount*100),
  _frustumGrid(scene->cam())
{
}

void PhotonTracer::clearCache()
{
    _directCache.clear();
    _indirectCache.clear();
}

static inline Vec3f exponentialIntegral(Vec3f a, Vec3f b, float t0, float t1)
{
    return (FastMath::exp(-a - b*t0) - FastMath::exp(-a - b*t1))/b;
}

static inline bool intersectBeam1D(const PhotonBeam &beam, const Ray &ray, const Vec3pf *bounds,
        float tMin, float tMax, float radius, float &invSinTheta, float &t)
{
    Vec3f l = beam.p0 - ray.pos();
    Vec3f u = l.cross(beam.dir).normalized();

    Vec3f n = beam.dir.cross(u);
    t = n.dot(l)/n.dot(ray.dir());
    Vec3f hitPoint = ray.pos() + ray.dir()*t;

    invSinTheta = 1.0f/std::sqrt(max(0.0f, 1.0f - sqr(ray.dir().dot(beam.dir))));
    if (std::abs(u.dot(hitPoint - beam.p0)) > radius)
        return false;

    if (bounds) {
        int majorAxis = std::abs(beam.dir).maxDim();
        float intervalMin = min((*bounds)[majorAxis][0], (*bounds)[majorAxis][1]);
        float intervalMax = max((*bounds)[majorAxis][2], (*bounds)[majorAxis][3]);

        if (hitPoint[majorAxis] < intervalMin || hitPoint[majorAxis] > intervalMax)
            return false;
    }

    if (t < tMin || t > tMax)
        return false;

    float s = beam.dir.dot(hitPoint - beam.p0);
    if (s < 0.0f || s > beam.length)
        return false;

    return true;
}
static inline bool intersectPlane0D(const Ray &ray, float tMin, float tMax, Vec3f p0, Vec3f p1, Vec3f p2,
        float &invDet, float &farT, Vec2f &uv)
{
    Vec3f e1 = p1 - p0;
    Vec3f e2 = p2 - p0;
    Vec3f P = ray.dir().cross(e2);
    float det = e1.dot(P);
    if (std::abs(det) < 1e-5f)
        return false;

    invDet = 1.0f/det;
    Vec3f T = ray.pos() - p0;
    float u = T.dot(P)*invDet;
    if (u < 0.0f || u > 1.0f)
        return false;

    Vec3f Q = T.cross(e1);
    float v = ray.dir().dot(Q)*invDet;
    if (v < 0.0f || v > 1.0f)
        return false;

    float maxT = e2.dot(Q)*invDet;
    if (maxT <= tMin || maxT >= tMax)
        return false;

    farT = maxT;
    uv = Vec2f(u, v);
    return true;
}
static inline bool intersectPlane1D(const Ray &ray, float minT, float maxT, Vec3f p0, Vec3f u, Vec3f v, Vec3f w,
        Vec3f &o, Vec3f &d, float &tMin, float &tMax)
{
    o = ray.pos() - p0;
    d = ray.dir();

    o = Vec3f(u.dot(o), v.dot(o), w.dot(o));
    d = Vec3f(u.dot(d), v.dot(d), w.dot(d));
    Vec3f invD = 1.0f/d;

    Vec3f t0 = -o*invD;
    Vec3f t1 = t0 + invD;

    float ttMin = max(min(t0, t1).max(), minT);
    float ttMax = min(max(t0, t1).min(), maxT);

    if (ttMin <= ttMax) {
        tMin = ttMin;
        tMax = ttMax;
        return true;
    }
    return false;
}

static bool evalBeam1D(const PhotonBeam &beam, PathSampleGenerator &sampler, const Ray &ray, const Medium *medium,
        const Vec3pf *bounds, float tMin, float tMax, float radius, Vec3f &beamEstimate)
{
    float invSinTheta, t;
    if (intersectBeam1D(beam, ray, bounds, tMin, tMax, radius, invSinTheta, t)) {
        Vec3f hitPoint = ray.pos() + ray.dir()*t;

        Ray mediumQuery(ray);
        mediumQuery.setFarT(t);
        beamEstimate += medium->sigmaT(hitPoint)*invSinTheta/(2.0f*radius)
                *medium->phaseFunction(hitPoint)->eval(beam.dir, -ray.dir())
                *medium->transmittance(sampler, mediumQuery, true, false)*beam.power;

        return true;
    }

    return false;
}
static bool evalPlane0D(const PhotonPlane0D &p, PathSampleGenerator &sampler, const Ray &ray, const Medium *medium,
        const TraceableScene *scene, float tMin, float tMax, Vec3f &beamEstimate)
{
    Vec2f uv;
    float invDet, t;
    if (intersectPlane0D(ray, tMin, tMax, p.p0, p.p1, p.p3, invDet, t, uv)) {
        Vec3f hitPoint = ray.pos() + ray.dir()*t;

        Ray shadowRay = Ray(hitPoint, -p.d1, 0.0f, p.l1*uv.y());
        if (!scene->occluded(shadowRay)) {
            Ray mediumQuery(ray);
            mediumQuery.setFarT(t);
            beamEstimate += sqr(medium->sigmaT(hitPoint))*std::abs(invDet)
                    *medium->phaseFunction(hitPoint)->eval(p.d1, -ray.dir())
                    *medium->transmittance(sampler, mediumQuery, true, false)*p.power;

            return true;
        }
    }
    return false;
}
template<typename ShadowCache>
bool evalPlane1D(const PhotonPlane1D &p, PathSampleGenerator &sampler, const Ray &ray, const Medium *medium,
        const TraceableScene *_scene, float tMin, float tMax, uint32 photonIdx,
        ShadowCache &shadowCache, Vec3f &beamEstimate)
{
    Vec3f o, d;
    float minT, maxT;
    if (intersectPlane1D(ray, tMin, tMax, p.p, p.invU, p.invV, p.invW, o, d, minT, maxT)) {
        float t = lerp(minT, maxT, sampler.next1D());
        Vec3f uvw = o + d*t;
        if (uvw.min() < 0.0f || uvw.max() > 1.0f)
            return false;

        Vec3f d0 = p.a*2.0f;
        Vec3f d1 = p.b*2.0f;
        Vec3f v0 = p.p + p.c;
        Vec3f v1 = v0 + uvw.x()*d0;
        Vec3f v2 = v1 + uvw.y()*d1;

        Vec3f sigmaT = medium->sigmaT(v2);
        Vec3f controlVariate = exponentialIntegral(Vec3f(0.0f), sigmaT, minT, maxT);

        float dist = shadowCache.hitDistance(photonIdx, uint32(p.binCount*uvw.x()), [&]() {
            Ray shadowRay = Ray(v1, p.d1, 0.0f, p.l1);
            return _scene->hitDistance(shadowRay);
        });

        if (dist < uvw.y()*p.l1*0.99f) {
            Ray mediumQuery(ray);
            mediumQuery.setFarT(t);

            controlVariate -= medium->transmittance(sampler, mediumQuery, true, false)*(maxT - minT);
        }

        beamEstimate += sqr(medium->sigmaT(v2))*medium->phaseFunction(v2)->eval(p.d1, -ray.dir())*p.power*controlVariate;
        return true;
    }
    return false;
}

void PhotonTracer::evalPrimaryRays(const PhotonBeam *beams, const PhotonPlane0D *planes0D, const PhotonPlane1D *planes1D,
            uint32 start, uint32 end, float radius, const Ray *depthBuffer, PathSampleGenerator &sampler, float scale)
{
    const Medium *medium = _scene->cam().medium().get();
    auto splatBuffer = _scene->cam().splatBuffer();
    Vec3f pos = _scene->cam().pos();

    int minBounce = _settings.minBounces - 1;
    int maxBounce = _settings.maxBounces - 1;

    for (uint32 i = start; i < end; ++i) {
        if (beams[i].valid && beams[i].bounce >= minBounce && beams[i].bounce < maxBounce) {
            const PhotonBeam &b = beams[i];
            Vec3f u = (b.p0 - pos).cross(b.dir).normalized();

            _frustumGrid.binBeam(b.p0, b.p1, u, radius, [&](uint32 x, uint32 y, uint32 idx) {
                const Ray &ray = depthBuffer[idx];
                Vec3f value(0.0f);
                if (evalBeam1D(b, sampler, ray, medium, nullptr, ray.nearT(), ray.farT(), radius, value))
                    splatBuffer->splat(Vec2u(x, y), value*scale);
            });
        }

        if (planes0D && planes0D[i].valid && planes0D[i].bounce >= minBounce && planes0D[i].bounce < maxBounce) {
            const PhotonPlane0D &p = planes0D[i];

            _frustumGrid.binPlane(p.p0, p.p1, p.p2, p.p3, [&](uint32 x, uint32 y, uint32 idx) {
                const Ray &ray = depthBuffer[idx];
                Vec3f value(0.0f);
                if (evalPlane0D(p, sampler, ray, medium, _scene, ray.nearT(), ray.farT(), value))
                    splatBuffer->splat(Vec2u(x, y), value*scale);
            });
        }

        if (planes1D && planes1D[i].valid && planes1D[i].bounce >= minBounce && planes1D[i].bounce < maxBounce) {
            const PhotonPlane1D &p = planes1D[i];

            _frustumGrid.binPlane1D(p.center, p.a, p.b, p.c, [&](uint32 x, uint32 y, uint32 idx) {
                const Ray &ray = depthBuffer[idx];
                Vec3f value(0.0f);
                if (evalPlane1D(p, sampler, ray, medium, _scene, ray.nearT(), ray.farT(), i, _directCache, value))
                    splatBuffer->splat(Vec2u(x, y), value*scale);
            });
        }
    }
}

Vec3f PhotonTracer::traceSensorPath(Vec2u pixel, const KdTree<Photon> &surfaceTree,
        const KdTree<VolumePhoton> *mediumTree, const Bvh::BinaryBvh *mediumBvh, const GridAccel *mediumGrid,
        const PhotonBeam *beams, const PhotonPlane0D *planes0D, const PhotonPlane1D *planes1D, PathSampleGenerator &sampler,
        float gatherRadius, float volumeGatherRadius,
        PhotonMapSettings::VolumePhotonType photonType, Ray &depthRay, bool useFrustumGrid)
{
    _mailIdx++;

    PositionSample point;
    if (!_scene->cam().samplePosition(sampler, point))
        return Vec3f(0.0f);
    DirectionSample direction;
    if (!_scene->cam().sampleDirection(sampler, point, pixel, direction))
        return Vec3f(0.0f);

    Vec3f throughput = point.weight*direction.weight;
    Ray ray(point.p, direction.d);
    ray.setPrimaryRay(true);

    IntersectionTemporary data;
    IntersectionInfo info;
    const Medium *medium = _scene->cam().medium().get();
    const bool includeSurfaces = _settings.includeSurfaces;

    Vec3f result(0.0f);
    int bounce = 0;
    bool didHit = _scene->intersect(ray, data, info);

    depthRay = ray;

    while ((medium || didHit) && bounce < _settings.maxBounces) {
        bounce++;

        if (medium) {
            if (bounce > 1 || !useFrustumGrid) {
                Vec3f estimate(0.0f);

                auto pointContribution = [&](const VolumePhoton &p, float t, float distSq) {
                    int fullPathBounce = bounce + p.bounce - 1;
                    if (fullPathBounce < _settings.minBounces || fullPathBounce >= _settings.maxBounces)
                        return;

                    Ray mediumQuery(ray);
                    mediumQuery.setFarT(t);
                    estimate += (3.0f*INV_PI*sqr(1.0f - distSq/p.radiusSq))/p.radiusSq
                            *medium->phaseFunction(p.pos)->eval(p.dir, -ray.dir())
                            *medium->transmittance(sampler, mediumQuery, true, false)*p.power;
                };
                auto beamContribution = [&](uint32 photonIndex, const Vec3pf *bounds, float tMin, float tMax) {
                    const PhotonBeam &beam = beams[photonIndex];
                    int fullPathBounce = bounce + beam.bounce;
                    if (fullPathBounce < _settings.minBounces || fullPathBounce >= _settings.maxBounces)
                        return;

                    evalBeam1D(beam, sampler, ray, medium, bounds, tMin, tMax, volumeGatherRadius, estimate);
                };
                auto planeContribution = [&](uint32 photon, const Vec3pf *bounds, float tMin, float tMax) {
                    int photonBounce = beams[photon].valid ? beams[photon].bounce : (planes0D ? planes0D[photon].bounce : planes1D[photon].bounce);
                    int fullPathBounce = bounce + photonBounce;
                    if (fullPathBounce < _settings.minBounces || fullPathBounce >= _settings.maxBounces)
                        return;

                    if (beams[photon].valid)
                        evalBeam1D(beams[photon], sampler, ray, medium, bounds, tMin, tMax, volumeGatherRadius, estimate);
                    else if (photonType == PhotonMapSettings::VOLUME_PLANES_1D)
                        evalPlane1D(planes1D[photon], sampler, ray, medium, _scene, tMin, tMax, photon, _indirectCache, estimate);
                    else
                        evalPlane0D(planes0D[photon], sampler, ray, medium, _scene, tMin, tMax, estimate);
                };


                if (photonType == PhotonMapSettings::VOLUME_POINTS) {
                    mediumTree->beamQuery(ray.pos(), ray.dir(), ray.farT(), pointContribution);
                } else if (photonType == PhotonMapSettings::VOLUME_BEAMS) {
                    if (mediumBvh) {
                        mediumBvh->trace(ray, [&](Ray &ray, uint32 photonIndex, float /*tMin*/, const Vec3pf &bounds) {
                            beamContribution(photonIndex, &bounds, ray.nearT(), ray.farT());
                        });
                    } else {
                        mediumGrid->trace(ray, [&](uint32 photonIndex, float tMin, float tMax) {
                            beamContribution(photonIndex, nullptr, tMin, tMax);
                        });
                    }
                } else if (photonType == PhotonMapSettings::VOLUME_PLANES || photonType == PhotonMapSettings::VOLUME_PLANES_1D) {
                    if (mediumBvh) {
                        mediumBvh->trace(ray, [&](Ray &ray, uint32 photonIndex, float /*tMin*/, const Vec3pf &bounds) {
                            planeContribution(photonIndex, &bounds, ray.nearT(), ray.farT());
                        });
                    } else {
                        mediumGrid->trace(ray, [&](uint32 photonIndex, float /*tMin*/, float /*tMax*/) {
                            if (_mailboxes[photonIndex] == _mailIdx)
                                return;
                            _mailboxes[photonIndex] = _mailIdx;
                            planeContribution(photonIndex, nullptr, ray.nearT(), ray.farT());
                        });
                    }
                }

                result += throughput*estimate;
            }
            throughput *= medium->transmittance(sampler, ray, true, true);
        }
        if (!didHit || !includeSurfaces)
            break;

        const Bsdf &bsdf = *info.bsdf;

        SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler);

        Vec3f transparency = bsdf.eval(event.makeForwardEvent(), false);
        float transparencyScalar = transparency.avg();

        Vec3f wo;
        if (sampler.nextBoolean(transparencyScalar)) {
            wo = ray.dir();
            throughput *= transparency/transparencyScalar;
        } else {
            event.requestedLobe = BsdfLobes::SpecularLobe;
            if (!bsdf.sample(event, false))
                break;

            wo = event.frame.toGlobal(event.wo);

            throughput *= event.weight;
        }

        bool geometricBackside = (wo.dot(info.Ng) < 0.0f);
        medium = info.primitive->selectMedium(medium, geometricBackside);

        ray = ray.scatter(ray.hitpoint(), wo, info.epsilon);

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }

    if (!includeSurfaces)
        return result;

    if (!didHit) {
        if (!medium && bounce > _settings.minBounces && _scene->intersectInfinites(ray, data, info))
            result += throughput*info.primitive->evalDirect(data, info);
        return result;
    }
    if (info.primitive->isEmissive() && bounce > _settings.minBounces)
        result += throughput*info.primitive->evalDirect(data, info);

    int count = surfaceTree.nearestNeighbours(ray.hitpoint(), _photonQuery.get(), _distanceQuery.get(),
            _settings.gatherCount, gatherRadius);
    if (count == 0)
        return result;

    const Bsdf &bsdf = *info.bsdf;
    SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler);

    Vec3f surfaceEstimate(0.0f);
    for (int i = 0; i < count; ++i) {
        int fullPathBounce = bounce + _photonQuery[i]->bounce - 1;
        if (fullPathBounce < _settings.minBounces || fullPathBounce >= _settings.maxBounces)
            continue;

        event.wo = event.frame.toLocal(-_photonQuery[i]->dir);
        // Asymmetry due to shading normals already compensated for when storing the photon,
        // so we don't use the adjoint BSDF here
        surfaceEstimate += _photonQuery[i]->power*bsdf.eval(event, false)/std::abs(event.wo.z());
    }
    float radiusSq = count == int(_settings.gatherCount) ? _distanceQuery[0] : gatherRadius*gatherRadius;
    result += throughput*surfaceEstimate*(INV_PI/radiusSq);

    return result;
}

void PhotonTracer::tracePhotonPath(SurfacePhotonRange &surfaceRange, VolumePhotonRange &volumeRange,
        PathPhotonRange &pathRange, PathSampleGenerator &sampler)
{
    float lightPdf;
    const Primitive *light = chooseLightAdjoint(sampler, lightPdf);
    const Medium *medium = light->extMedium().get();

    PositionSample point;
    if (!light->samplePosition(sampler, point))
        return;
    DirectionSample direction;
    if (!light->sampleDirection(sampler, point, direction))
        return;

    Ray ray(point.p, direction.d);
    Vec3f throughput(point.weight*direction.weight/lightPdf);

    if (!pathRange.full()) {
        PathPhoton &p = pathRange.addPhoton();
        p.pos = point.p;
        p.power = throughput;
        p.setPathInfo(0, true);
    }

    SurfaceScatterEvent event;
    IntersectionTemporary data;
    IntersectionInfo info;
    Medium::MediumState state;
    state.reset();
    Vec3f emission(0.0f);

    bool tracePlanes = (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES) ||
                       (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES_1D);
    bool useLowOrder = _settings.lowOrderScattering || _settings.volumePhotonType != PhotonMapSettings::VOLUME_POINTS;
    int bounce = 0;
    int bounceSinceSurface = 0;
    bool wasSpecular = true;
    bool didHit = _scene->intersect(ray, data, info);
    while ((didHit || medium) && bounce < _settings.maxBounces - 1) {
        bool hitSurface = didHit;
        bounce++;
        bounceSinceSurface++;

        Vec3f continuedThroughput = throughput;
        if (medium) {
            MediumSample mediumSample;
            if (!medium->sampleDistance(sampler, ray, state, mediumSample))
                break;
            continuedThroughput *= mediumSample.continuedWeight;
            throughput *= mediumSample.weight;
            hitSurface = mediumSample.exited;

            if (!hitSurface && (useLowOrder || bounceSinceSurface > 1) && !volumeRange.full()) {
                VolumePhoton &p = volumeRange.addPhoton();
                p.pos = mediumSample.p;
                p.dir = ray.dir();
                p.power = throughput;
                p.bounce = bounce;
            }

            if ((!hitSurface || tracePlanes) && !pathRange.full()) {
                pathRange.nextPtr()[-1].sampledLength = mediumSample.continuedT;
                PathPhoton &p = pathRange.addPhoton();
                p.pos = mediumSample.p;
                p.power = continuedThroughput;
                p.setPathInfo(bounce, false);
            }

            Ray continuedRay;
            PhaseSample phaseSample;
            if (!hitSurface || tracePlanes) {
                if (!mediumSample.phase->sample(sampler, ray.dir(), phaseSample))
                    break;
                continuedRay = ray.scatter(mediumSample.p, phaseSample.w, 0.0f);
                continuedRay.setPrimaryRay(false);
            }

            if (!hitSurface) {
                ray = continuedRay;
                throughput *= phaseSample.weight;
            } else if (tracePlanes) {
                Medium::MediumState continuedState = state;
                if (!medium->sampleDistance(sampler, continuedRay, continuedState, mediumSample))
                    break;
                if (!pathRange.full()) {
                    pathRange.nextPtr()[-1].sampledLength = mediumSample.continuedT;
                    PathPhoton &p = pathRange.addPhoton();
                    p.pos = mediumSample.p;
                    p.power = throughput*mediumSample.weight*phaseSample.weight;
                    p.setPathInfo(bounce + 1, true);
                }
            }
        }

        if (hitSurface) {
            if (!info.bsdf->lobes().isPureSpecular() && !surfaceRange.full()) {
                Photon &p = surfaceRange.addPhoton();
                p.pos = info.p;
                p.dir = ray.dir();
                p.power = throughput*std::abs(info.Ns.dot(ray.dir())/info.Ng.dot(ray.dir()));
                p.bounce = bounce;
            }
            if (!pathRange.full()) {
                PathPhoton &p = pathRange.addPhoton();
                p.pos = info.p;
                p.power = continuedThroughput;
                p.setPathInfo(bounce, true);
            }
        }

        if (volumeRange.full() && surfaceRange.full() && pathRange.full())
            break;

        if (hitSurface) {
            event = makeLocalScatterEvent(data, info, ray, &sampler);
            if (!handleSurface(event, data, info, medium, bounce,
                    true, false, ray, throughput, emission, wasSpecular, state))
                break;
            bounceSinceSurface = 0;
        }

        if (throughput.max() == 0.0f)
            break;

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
}

}
