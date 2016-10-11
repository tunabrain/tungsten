#include "PhotonMapIntegrator.hpp"
#include "PhotonTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

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

void PhotonMapIntegrator::tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId)
{
    SubTaskData &data = _taskData[taskId];
    PathSampleGenerator &sampler = *_samplers[taskId];

    uint32 photonBase    = intLerp(0, _settings.photonCount, taskId + 0, numSubTasks);
    uint32 photonsToCast = intLerp(0, _settings.photonCount, taskId + 1, numSubTasks) - photonBase;

    uint32 totalSurfaceCast = 0;
    uint32 totalVolumeCast = 0;
    uint32 totalPathsCast = 0;
    for (uint32 i = 0; i < photonsToCast; ++i) {
        sampler.startPath(0, photonBase + i);
        _tracers[threadId]->tracePhoton(
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
    }

    _totalTracedSurfacePhotons += totalSurfaceCast;
    _totalTracedVolumePhotons += totalVolumeCast;
    _totalTracedPathPhotons += totalPathsCast;
}

void PhotonMapIntegrator::tracePixels(uint32 tileId, uint32 threadId)
{
    int spp = _nextSpp - _currentSpp;

    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;

            for (int i = 0; i < spp; ++i) {
                tile.sampler->startPath(pixelIndex, _currentSpp + i);
                Vec3f c = _tracers[threadId]->traceSample(pixel,
                    *_surfaceTree,
                    _volumeTree.get(),
                    _beamBvh.get(),
                    &_pathPhotons[0],
                    *tile.sampler,
                    _settings.gatherRadius,
                    _settings.volumeGatherRadius
                );
                _scene->cam().colorBuffer()->addSample(pixel, c);
            }
        }
    }
}

template<typename PhotonType>
uint32 streamCompact(std::vector<PhotonRange<PhotonType>> &ranges)
{
    uint32 tail = 0;
    for (size_t i = 0; i < ranges.size(); ++i) {
        uint32 gap = ranges[i].end() - ranges[i].next();

        for (size_t t = i + 1; t < ranges.size() && gap > 0; ++t) {
            uint32 copyCount = min(gap, ranges[t].next() - ranges[t].start());
            if (copyCount > 0) {
                std::memcpy(
                    ranges[i].nextPtr(),
                    ranges[t].nextPtr() - copyCount,
                    copyCount*sizeof(PhotonType)
                );
            }
            ranges[i].bumpNext( int(copyCount));
            ranges[t].bumpNext(-int(copyCount));
            gap -= copyCount;
        }

        tail = ranges[i].next();
        if (gap > 0)
            break;
    }

    return tail;
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

void PhotonMapIntegrator::buildBeamBvh(std::vector<PathPhotonRange> pathRanges)
{
    Bvh::PrimVector beams;
    uint32 tail = streamCompact(pathRanges);
    for (uint32 i = 0; i < tail; ++i) {
        _pathPhotons[i].power *= (1.0/_totalTracedPathPhotons);
        if (_pathPhotons[i].bounce() == 0)
            continue;

        Vec3f dir = _pathPhotons[i].pos - _pathPhotons[i - 1].pos;
        Vec3f minExtend = Vec3f(_settings.volumeGatherRadius);
        for (int j = 0; j < 3; ++j)
            minExtend[j] = std::copysign(minExtend[j], dir[j]);

        _pathPhotons[i - 1].length = dir.length();
        _pathPhotons[i - 1].dir = dir/_pathPhotons[i - 1].length;

        Vec3f absDir = std::abs(dir);
        int majorAxis = absDir.maxDim();
        int numSteps = min(64, max(1, int(absDir[majorAxis]*16.0f)));
        for (int j = 0; j < numSteps; ++j) {
            Vec3f p0 = _pathPhotons[i - 1].pos + dir*(j + 0)/numSteps;
            Vec3f p1 = _pathPhotons[i - 1].pos + dir*(j + 1)/numSteps;
            for (int k = 0; k < 3; ++k) {
                if (k != majorAxis || j ==            0) p0[k] -= minExtend[k];
                if (k != majorAxis || j == numSteps - 1) p1[k] += minExtend[k];
            }
            Box3f bounds;
            bounds.grow(p0);
            bounds.grow(p1);

            beams.emplace_back(Bvh::Primitive(bounds, bounds.center(), i - 1));
        }
    }

    _beamBvh.reset(new Bvh::BinaryBvh(std::move(beams), 1));
}

void PhotonMapIntegrator::buildPhotonDataStructures()
{
    std::vector<SurfacePhotonRange> surfaceRanges;
    std::vector<VolumePhotonRange> volumeRanges;
    std::vector<PathPhotonRange> pathRanges;
    for (const SubTaskData &data : _taskData) {
        surfaceRanges.emplace_back(data.surfaceRange);
        volumeRanges.emplace_back(data.volumeRange);
        pathRanges.emplace_back(data.pathRange);
    }
    _surfaceTree = streamCompactAndBuild(surfaceRanges, _surfacePhotons, _totalTracedSurfacePhotons);
    if (!_volumePhotons.empty()) {
        _volumeTree = streamCompactAndBuild(volumeRanges, _volumePhotons, _totalTracedVolumePhotons);
        _volumeTree->buildVolumeHierarchy(false, 1.0f);
    } else if (!_pathPhotons.empty()) {
        buildBeamBvh(std::move(pathRanges));
    }
}

void PhotonMapIntegrator::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    _settings.fromJson(v);
}

rapidjson::Value PhotonMapIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void PhotonMapIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _sampler = UniformSampler(MathUtil::hash32(seed));
    _currentSpp = 0;
    _totalTracedSurfacePhotons = 0;
    _totalTracedVolumePhotons  = 0;
    _totalTracedPathPhotons  = 0;
    _scene = &scene;
    advanceSpp();
    scene.cam().requestColorBuffer();

    _surfacePhotons.resize(_settings.photonCount);
    if (!_scene->media().empty()) {
        if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_POINTS)
            _volumePhotons.resize(_settings.volumePhotonCount);
        else if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_BEAMS)
            _pathPhotons.resize(_settings.volumePhotonCount);
    }

    int numThreads = ThreadUtils::pool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        uint32 surfaceRangeStart = intLerp(0, uint32(     _surfacePhotons.size()), i + 0, numThreads);
        uint32 surfaceRangeEnd   = intLerp(0, uint32(     _surfacePhotons.size()), i + 1, numThreads);
        uint32  volumeRangeStart = intLerp(0, uint32(_settings.volumePhotonCount), i + 0, numThreads);
        uint32  volumeRangeEnd   = intLerp(0, uint32(_settings.volumePhotonCount), i + 1, numThreads);
        _taskData.emplace_back(SubTaskData{
            SurfacePhotonRange(&_surfacePhotons[0], surfaceRangeStart, surfaceRangeEnd),
            VolumePhotonRange(_volumePhotons.empty() ? nullptr : &_volumePhotons[0], volumeRangeStart, volumeRangeEnd),
              PathPhotonRange(  _pathPhotons.empty() ? nullptr : &  _pathPhotons[0], volumeRangeStart, volumeRangeEnd)
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

    diceTiles();
}

void PhotonMapIntegrator::teardownAfterRender()
{
    _group.reset();
    _surfaceTree.reset();
    _volumeTree.reset();

    _surfacePhotons.clear();
     _volumePhotons.clear();
          _taskData.clear();
           _tracers.clear();

    _surfacePhotons.shrink_to_fit();
     _volumePhotons.shrink_to_fit();
          _taskData.shrink_to_fit();
           _tracers.shrink_to_fit();
}

void PhotonMapIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    using namespace std::placeholders;
    if (!_surfaceTree) {
        _group = ThreadUtils::pool->enqueue(
            std::bind(&PhotonMapIntegrator::tracePhotons, this, _1, _2, _3),
            _tracers.size(),
            [&, completionCallback]() {
                buildPhotonDataStructures();
                completionCallback();
            }
        );
    } else {
        _group = ThreadUtils::pool->enqueue(
            std::bind(&PhotonMapIntegrator::tracePixels, this, _1, _3),
            _tiles.size(),
            [&, completionCallback]() {
                _currentSpp = _nextSpp;
                advanceSpp();
                completionCallback();
            }
        );
    }
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
