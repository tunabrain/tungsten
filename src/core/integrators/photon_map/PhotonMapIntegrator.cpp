#include "PhotonMapIntegrator.hpp"
#include "PhotonTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/PinholeCamera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "bvh/BinaryBvh.hpp"

namespace Tungsten {

CONSTEXPR uint32 PhotonMapIntegrator::TileSize;

PhotonMapIntegrator::PhotonMapIntegrator()
: _w(0),
  _h(0),
  _sampler(0xBA5EBA11)
{
}

PhotonMapIntegrator::~PhotonMapIntegrator()
{
}

void PhotonMapIntegrator::diceTiles()
{
    for (uint32 y = 0; y < _h; y += TileSize) {
        for (uint32 x = 0; x < _w; x += TileSize) {
            _tiles.emplace_back(
                x,
                y,
                min(TileSize, _w - x),
                min(TileSize, _h - y),
                _scene->rendererSettings().useSobol() ?
                    std::unique_ptr<PathSampleGenerator>(new SobolPathSampler(MathUtil::hash32(_sampler.nextI()))) :
                    std::unique_ptr<PathSampleGenerator>(new UniformPathSampler(MathUtil::hash32(_sampler.nextI())))
            );
        }
    }
}

void PhotonMapIntegrator::saveState(OutputStreamHandle &/*out*/)
{
}

void PhotonMapIntegrator::loadState(InputStreamHandle &/*in*/)
{
}

void PhotonMapIntegrator::tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId, uint32 sampleBase)
{
    SubTaskData &data = _taskData[taskId];
    PathSampleGenerator &sampler = *_samplers[taskId];

    uint32 photonBase    = intLerp(0, _settings.photonCount, taskId + 0, numSubTasks);
    uint32 photonsToCast = intLerp(0, _settings.photonCount, taskId + 1, numSubTasks) - photonBase;

    uint32 totalSurfaceCast = 0;
    uint32 totalVolumeCast = 0;
    uint32 totalPathsCast = 0;
    for (uint32 i = 0; i < photonsToCast; ++i) {
        sampler.startPath(0, sampleBase + photonBase + i);
        _tracers[threadId]->tracePhotonPath(
            data.surfaceRange,
            data.volumeRange,
            data.pathRange,
            sampler
        );
        if (!data.surfaceRange.full())
            totalSurfaceCast++;
        if (!data.volumeRange.full())
            totalVolumeCast++;
        if (!data.pathRange.full())
            totalPathsCast++;
        if (data.surfaceRange.full() && data.volumeRange.full() && data.pathRange.full())
            break;

        if (_group->isAborting())
                break;
    }

    _totalTracedSurfacePaths += totalSurfaceCast;
    _totalTracedVolumePaths += totalVolumeCast;
    _totalTracedPaths += totalPathsCast;
}

void PhotonMapIntegrator::tracePixels(uint32 tileId, uint32 threadId, float surfaceRadius, float volumeRadius)
{
    int spp = _nextSpp - _currentSpp;

    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;

            Ray dummyRay;
            Ray *depthRay = _depthBuffer ? &_depthBuffer[pixel.x() + pixel.y()*_w] : &dummyRay;
            for (int i = 0; i < spp; ++i) {
                tile.sampler->startPath(pixelIndex, _currentSpp + i);
                Vec3f c = _tracers[threadId]->traceSensorPath(pixel,
                    *_surfaceTree,
                    _volumeTree.get(),
                    _volumeBvh.get(),
                    _volumeGrid.get(),
                    _beams.get(),
                    _planes0D.get(),
                    _planes1D.get(),
                    *tile.sampler,
                    surfaceRadius,
                    volumeRadius,
                    _settings.volumePhotonType,
                    *depthRay,
                    _useFrustumGrid
                );
                _scene->cam().colorBuffer()->addSample(pixel, c);
            }
            if (_group->isAborting())
                break;
        }
    }
}

template<typename PhotonType>
std::unique_ptr<KdTree<PhotonType>> streamCompactAndBuild(std::vector<PhotonRange<PhotonType>> ranges,
        std::vector<PhotonType> &photons, uint32 totalTraced)
{
    uint32 tail = streamCompact(ranges);

    float scale = 1.0f/totalTraced;
    for (uint32 i = 0; i < tail; ++i)
        photons[i].power *= scale;

    return std::unique_ptr<KdTree<PhotonType>>(new KdTree<PhotonType>(&photons[0], tail));
}

static void precomputeBeam(PhotonBeam &beam, const PathPhoton &p0, const PathPhoton &p1)
{
    beam.p0 = p0.pos;
    beam.p1 = p1.pos;
    beam.dir = p0.dir;
    beam.length = p0.length;
    beam.power = p1.power;
    beam.bounce = p0.bounce();
    beam.valid = true;
}
static void precomputePlane0D(PhotonPlane0D &plane, const PathPhoton &p0, const PathPhoton &p1, const PathPhoton &p2)
{
    Vec3f d1 = p1.dir*p1.sampledLength;
    plane = PhotonPlane0D{
        p0.pos, p1.pos, p1.pos + d1, p0.pos + d1,
        p0.length*p1.sampledLength*p2.power,
        p1.dir,
        p1.sampledLength,
        int(p1.bounce()),
        true
    };
}
static void precomputePlane1D(PhotonPlane1D &plane, const PathPhoton &p0, const PathPhoton &p1, const PathPhoton &p2, float radius)
{
    Vec3f a = p1.pos - p0.pos;
    Vec3f b = p1.dir*p1.sampledLength;
    Vec3f c = 2.0f*a.cross(p1.dir).normalized()*radius;
    float det = std::abs(a.dot(b.cross(c)));

    if (std::isnan(c.sum()) || det < 1e-8f)
        return;

    float invDet = 1.0f/det;
    Vec3f u = invDet*b.cross(c);
    Vec3f v = invDet*c.cross(a);
    Vec3f w = invDet*a.cross(b);

    plane.p = p0.pos - c*0.5f;
    plane.invDet = invDet;
    plane.invU = u;
    plane.invV = v;
    plane.invW = w;
    plane.binCount = a.length()/(2.0f*radius);
    plane.valid = true;

    plane.center = p0.pos + a*0.5f + b*0.5f;
    plane.a = a*0.5f;
    plane.b = b*0.5f;
    plane.c = c*0.5f;

    plane.d1 = p1.dir;
    plane.l1 = p1.sampledLength;
    plane.power = p0.length*p1.sampledLength*p2.power*std::abs(invDet);
    plane.bounce = p1.bounce();
}

static void insertDicedBeam(Bvh::PrimVector &beams, PhotonBeam &beam, uint32 i, const PathPhoton &p0, const PathPhoton &p1, float radius)
{
    precomputeBeam(beam, p0, p1);

    Vec3f absDir = std::abs(p0.dir);
    int majorAxis = absDir.maxDim();
    int numSteps = min(64, max(1, int(absDir[majorAxis]*16.0f)));

    Vec3f minExtend = Vec3f(radius);
    for (int j = 0; j < 3; ++j) {
        minExtend[j] = std::copysign(minExtend[j], p0.dir[j]);
        if (j != majorAxis)
            minExtend[j] /= std::sqrt(max(0.0f, 1.0f - sqr(p0.dir[j])));
    }
    for (int j = 0; j < numSteps; ++j) {
        Vec3f v0 = p0.pos + p0.dir*p0.length*(j + 0)/numSteps;
        Vec3f v1 = p0.pos + p0.dir*p0.length*(j + 1)/numSteps;
        for (int k = 0; k < 3; ++k) {
            if (k != majorAxis || j ==            0) v0[k] -= minExtend[k];
            if (k != majorAxis || j == numSteps - 1) v1[k] += minExtend[k];
        }
        Box3f bounds;
        bounds.grow(v0);
        bounds.grow(v1);

        beams.emplace_back(Bvh::Primitive(bounds, bounds.center(), i));
    }
}

void PhotonMapIntegrator::buildPointBvh(uint32 tail, float volumeRadiusScale)
{
    float radius = _settings.volumeGatherRadius*volumeRadiusScale;

    Bvh::PrimVector points;
    for (uint32 i = 0; i < tail; ++i) {
        Box3f bounds(_pathPhotons[i].pos);
        bounds.grow(radius);
        points.emplace_back(Bvh::Primitive(bounds, _pathPhotons[i].pos, i));
    }

    _volumeBvh.reset(new Bvh::BinaryBvh(std::move(points), 1));
}
void PhotonMapIntegrator::buildBeamBvh(uint32 tail, float volumeRadiusScale)
{
    float radius = _settings.volumeGatherRadius*volumeRadiusScale;

    Bvh::PrimVector beams;
    for (uint32 i = 0; i < tail; ++i) {
        if (_pathPhotons[i].bounce() == 0)
            continue;

        if (!_pathPhotons[i - 1].onSurface() || _settings.lowOrderScattering)
            insertDicedBeam(beams, _beams[i], i, _pathPhotons[i - 1], _pathPhotons[i], radius);
    }

    _volumeBvh.reset(new Bvh::BinaryBvh(std::move(beams), 1));
}
void PhotonMapIntegrator::buildPlaneBvh(uint32 tail, float volumeRadiusScale)
{
    float radius = _settings.volumeGatherRadius*volumeRadiusScale;

    Bvh::PrimVector planes;
    for (uint32 i = 0; i < tail; ++i) {
        const PathPhoton &p0 = _pathPhotons[i - 2];
        const PathPhoton &p1 = _pathPhotons[i - 1];
        const PathPhoton &p2 = _pathPhotons[i - 0];

        if (p2.bounce() > 0 && p2.bounce() > p1.bounce() && p1.onSurface() && _settings.lowOrderScattering)
            insertDicedBeam(planes, _beams[i], i, p1, p2, radius);
        if (p2.bounce() > 1 && !p1.onSurface() && p1.sampledLength > 0.0f) {
            if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES) {
                precomputePlane0D(_planes0D[i], p0, p1, p2);
                Box3f bounds = _planes0D[i].bounds();
                planes.emplace_back(Bvh::Primitive(bounds, bounds.center(), i));
            } else {
                precomputePlane1D(_planes1D[i], p0, p1, p2, radius);
                if (_planes1D[i].valid) {
                    Box3f bounds = _planes1D[i].bounds();
                    planes.emplace_back(Bvh::Primitive(bounds, bounds.center(), i));
                }
            }
        }
    }

    _volumeBvh.reset(new Bvh::BinaryBvh(std::move(planes), 1));
}

void PhotonMapIntegrator::buildBeamGrid(uint32 tail, float volumeRadiusScale)
{
    float radius = _settings.volumeGatherRadius*volumeRadiusScale;

    std::vector<GridAccel::Primitive> beams;
    for (uint32 i = 0; i < tail; ++i) {
        const PathPhoton &p0 = _pathPhotons[i - 1];
        const PathPhoton &p1 = _pathPhotons[i - 0];
        if (_pathPhotons[i].bounce() == 0)
            continue;

        if (!_pathPhotons[i - 1].onSurface() || _settings.lowOrderScattering) {
            precomputeBeam(_beams[i], p0, p1);
            beams.emplace_back(GridAccel::Primitive(i, p0.pos, p1.pos, Vec3f(0.0f), Vec3f(0.0f), radius, true));
        }
    }

    _volumeGrid.reset(new GridAccel(_scene->bounds(), _settings.gridMemBudgetKb, std::move(beams)));
}
void PhotonMapIntegrator::buildPlaneGrid(uint32 tail, float volumeRadiusScale)
{
    float radius = _settings.volumeGatherRadius*volumeRadiusScale;

    std::vector<GridAccel::Primitive> prims;
    for (uint32 i = 0; i < tail; ++i) {
        const PathPhoton &p0 = _pathPhotons[i - 2];
        const PathPhoton &p1 = _pathPhotons[i - 1];
        const PathPhoton &p2 = _pathPhotons[i - 0];

        if (p2.bounce() > 0 && p2.bounce() > p1.bounce() && p1.onSurface() && _settings.lowOrderScattering) {
            precomputeBeam(_beams[i], p1, p2);
            prims.emplace_back(GridAccel::Primitive(i, p1.pos, p2.pos, Vec3f(0.0f), Vec3f(0.0f), radius, true));
        }
        if (p2.bounce() > 1 && !p1.onSurface() && p1.sampledLength > 0.0f) {
            if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES) {
                precomputePlane0D(_planes0D[i], p0, p1, p2);
                prims.emplace_back(GridAccel::Primitive(i, _planes0D[i].p0, _planes0D[i].p1, _planes0D[i].p2, _planes0D[i].p3, 0.0f, false));
            } else {
                precomputePlane1D(_planes1D[i], p0, p1, p2, radius);
                if (_planes1D[i].valid) {
                    Vec3f p = _planes1D[i].center, a = _planes1D[i].a, b = _planes1D[i].b;
                    prims.emplace_back(GridAccel::Primitive(i, p - a - b, p + a - b, p + a + b, p - a + b, radius, false));
                }
            }
        }
    }

    _volumeGrid.reset(new GridAccel(_scene->bounds(), _settings.gridMemBudgetKb, std::move(prims)));
}

void PhotonMapIntegrator::buildPhotonDataStructures(float volumeRadiusScale)
{
    std::vector<SurfacePhotonRange> surfaceRanges;
    std::vector<VolumePhotonRange> volumeRanges;
    std::vector<PathPhotonRange> pathRanges;
    for (const SubTaskData &data : _taskData) {
        surfaceRanges.emplace_back(data.surfaceRange);
        volumeRanges.emplace_back(data.volumeRange);
        pathRanges.emplace_back(data.pathRange);
    }

    _surfaceTree = streamCompactAndBuild(surfaceRanges, _surfacePhotons, _totalTracedSurfacePaths);

    if (!_volumePhotons.empty()) {
        _volumeTree = streamCompactAndBuild(volumeRanges, _volumePhotons, _totalTracedVolumePaths);
        float volumeRadius = _settings.fixedVolumeRadius ? _settings.volumeGatherRadius : 1.0f;
        _volumeTree->buildVolumeHierarchy(_settings.fixedVolumeRadius, volumeRadius*volumeRadiusScale);
    } else if (!_pathPhotons.empty()) {
        uint32 tail = streamCompact(pathRanges);
        for (uint32 i = 0; i < tail; ++i)
            _pathPhotons[i].power *= (1.0/_totalTracedPaths);

        for (uint32 i = 0; i < tail; ++i) {
            if (_pathPhotons[i].bounce() > 0) {
                Vec3f dir = _pathPhotons[i].pos - _pathPhotons[i - 1].pos;
                _pathPhotons[i - 1].length = dir.length();
                _pathPhotons[i - 1].dir = dir/_pathPhotons[i - 1].length;
            }
        }

        _beams.reset(new PhotonBeam[tail]);
        for (uint32 i = 0; i < tail; ++i)
            _beams[i].valid = false;

        if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_BEAMS) {
            if (_settings.useGrid)
                buildBeamGrid(tail, volumeRadiusScale);
            else
                buildBeamBvh(tail, volumeRadiusScale);
        } else if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES || _settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES_1D) {
            if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES) {
                 _planes0D.reset(new PhotonPlane0D[tail]);
                for (uint32 i = 0; i < tail; ++i)
                    _planes0D[i].valid = false;
            }
            if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_PLANES_1D) {
                _planes1D.reset(new PhotonPlane1D[tail]);
                for (uint32 i = 0; i < tail; ++i)
                    _planes1D[i].valid = false;
            }

            if (_settings.useGrid)
                buildPlaneGrid(tail, volumeRadiusScale);
            else
                buildPlaneBvh(tail, volumeRadiusScale);
        }

        _pathPhotonCount = tail;
    }
}

void PhotonMapIntegrator::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    _settings.fromJson(value);
}

rapidjson::Value PhotonMapIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void PhotonMapIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _sampler = UniformSampler(MathUtil::hash32(seed));
    _currentSpp = 0;
    _totalTracedSurfacePaths = 0;
    _totalTracedVolumePaths  = 0;
    _totalTracedPaths        = 0;
    _pathPhotonCount         = 0;
    _scene = &scene;
    advanceSpp();
    scene.cam().requestColorBuffer();
    scene.cam().requestSplatBuffer();

    _useFrustumGrid = _settings.useFrustumGrid;
    if (_useFrustumGrid && !dynamic_cast<const PinholeCamera *>(&scene.cam())) {
        std::cout << "Warning: Frustum grid acceleration structure is only supported for a pinhole camera. "
                "Frustum grid will be disabled for this render." << std::endl;
        _useFrustumGrid = false;
    }

    if (_settings.includeSurfaces)
        _surfacePhotons.resize(_settings.photonCount);
    if (!_scene->media().empty()) {
        if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_POINTS)
            _volumePhotons.resize(_settings.volumePhotonCount);
        else
            _pathPhotons.resize(_settings.volumePhotonCount);
    }

    int numThreads = ThreadUtils::pool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        uint32 surfaceRangeStart = intLerp(0, uint32(     _surfacePhotons.size()), i + 0, numThreads);
        uint32 surfaceRangeEnd   = intLerp(0, uint32(     _surfacePhotons.size()), i + 1, numThreads);
        uint32  volumeRangeStart = intLerp(0, uint32(_settings.volumePhotonCount), i + 0, numThreads);
        uint32  volumeRangeEnd   = intLerp(0, uint32(_settings.volumePhotonCount), i + 1, numThreads);
        _taskData.emplace_back(SubTaskData{
            SurfacePhotonRange(_surfacePhotons.empty() ? nullptr : &_surfacePhotons[0], surfaceRangeStart, surfaceRangeEnd),
            VolumePhotonRange(  _volumePhotons.empty() ? nullptr : & _volumePhotons[0],  volumeRangeStart,  volumeRangeEnd),
              PathPhotonRange(    _pathPhotons.empty() ? nullptr : &   _pathPhotons[0],  volumeRangeStart,  volumeRangeEnd)
        });
        _samplers.emplace_back(_scene->rendererSettings().useSobol() ?
            std::unique_ptr<PathSampleGenerator>(new SobolPathSampler(MathUtil::hash32(_sampler.nextI()))) :
            std::unique_ptr<PathSampleGenerator>(new UniformPathSampler(MathUtil::hash32(_sampler.nextI())))
        );

        _tracers.emplace_back(new PhotonTracer(&scene, _settings, i));
    }

    Vec2u res = _scene->cam().resolution();
    _w = res.x();
    _h = res.y();

    if (_useFrustumGrid)
        _depthBuffer.reset(new Ray[_w*_h]);

    diceTiles();
}

void PhotonMapIntegrator::teardownAfterRender()
{
    _group.reset();
    _depthBuffer.reset();

    _beams.reset();
    _planes0D.reset();
    _planes1D.reset();

    _surfacePhotons.clear();
     _volumePhotons.clear();
       _pathPhotons.clear();
          _taskData.clear();
          _samplers.clear();
           _tracers.clear();

    _surfacePhotons.shrink_to_fit();
     _volumePhotons.shrink_to_fit();
       _pathPhotons.shrink_to_fit();
          _taskData.shrink_to_fit();
          _samplers.shrink_to_fit();
           _tracers.shrink_to_fit();

    _surfaceTree.reset();
    _volumeTree.reset();
    _volumeGrid.reset();
    _volumeBvh.reset();
}

void PhotonMapIntegrator::renderSegment(std::function<void()> completionCallback)
{
    using namespace std::placeholders;

    _scene->cam().setSplatWeight(1.0/_nextSpp);

    if (!_surfaceTree) {
        ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
            std::bind(&PhotonMapIntegrator::tracePhotons, this, _1, _2, _3, 0),
            _tracers.size(), [](){}
        ));

        buildPhotonDataStructures(1.0f);
    }

    ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
        std::bind(&PhotonMapIntegrator::tracePixels, this, _1, _3, _settings.gatherRadius, _settings.volumeGatherRadius),
        _tiles.size(), [](){}
    ));

    if (_useFrustumGrid) {
        ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
            [&](uint32 tracerId, uint32 numTracers, uint32) {
                uint32 start = intLerp(0, _pathPhotonCount, tracerId,     numTracers);
                uint32 end   = intLerp(0, _pathPhotonCount, tracerId + 1, numTracers);
                _tracers[tracerId]->evalPrimaryRays(_beams.get(), _planes0D.get(), _planes1D.get(),
                        start, end, _settings.volumeGatherRadius, _depthBuffer.get(), *_samplers[tracerId],
                        _nextSpp - _currentSpp);
            }, _tracers.size(), [](){}
        ));
    }

    _currentSpp = _nextSpp;
    advanceSpp();

    completionCallback();
}

void PhotonMapIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    _group = ThreadUtils::pool->enqueue([&, completionCallback](uint32, uint32, uint32) {
        renderSegment(completionCallback);
    }, 1, [](){});
}

void PhotonMapIntegrator::waitForCompletion()
{
    if (_group) {
        _group->wait();
        _group.reset();
    }
}

void PhotonMapIntegrator::abortRender()
{
    if (_group) {
        _group->abort();
        _group->wait();
        _group.reset();
    }
}

}
