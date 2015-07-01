#include "PhotonMapIntegrator.hpp"
#include "PhotonTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

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
    for (uint32 i = 0; i < photonsToCast; ++i) {
        sampler.startPath(taskId, photonBase + i);
        _tracers[threadId]->tracePhoton(
            data.surfaceRange,
            data.volumeRange,
            sampler
        );
        if (!data.surfaceRange.full())
            totalSurfaceCast++;
        if (!data.volumeRange.full())
            totalVolumeCast++;
        if (data.surfaceRange.full() && data.volumeRange.full())
            break;
    }

    _totalTracedSurfacePhotons += totalSurfaceCast;
    _totalTracedVolumePhotons += totalVolumeCast;
}

void PhotonMapIntegrator::tracePixels(uint32 tileId, uint32 threadId)
{
    int spp = _nextSpp - _currentSpp;

    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;

            Vec3f c(0.0f);
            for (int i = 0; i < spp; ++i) {
                tile.sampler->startPath(pixelIndex, _currentSpp + i);
                Vec3f s(_tracers[threadId]->traceSample(pixel,
                    *_surfaceTree,
                    _volumeTree.get(),
                    *tile.sampler,
                    _settings.gatherRadius
                ));
                c += s;
            }

            _scene->cam().addSamples(x + tile.x, y + tile.y, c, spp);
        }
    }
}

template<typename PhotonType>
std::unique_ptr<KdTree<PhotonType>> streamCompactAndBuild(std::vector<PhotonRange<PhotonType>> ranges,
        std::vector<PhotonType> &photons, uint32 totalTraced)
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

    float scale = 1.0f/totalTraced;
    for (uint32 i = 0; i < tail; ++i)
        photons[i].power *= scale;

    return std::unique_ptr<KdTree<PhotonType>>(new KdTree<PhotonType>(&photons[0], tail));
}

void PhotonMapIntegrator::buildPhotonDataStructures()
{
    std::vector<SurfacePhotonRange> surfaceRanges;
    std::vector<VolumePhotonRange> volumeRanges;
    for (const SubTaskData &data : _taskData) {
        surfaceRanges.emplace_back(data.surfaceRange);
        volumeRanges.emplace_back(data.volumeRange);
    }

    _surfaceTree = streamCompactAndBuild(surfaceRanges, _surfacePhotons, _totalTracedSurfacePhotons);
    if (!_volumePhotons.empty()) {
        _volumeTree = streamCompactAndBuild(volumeRanges, _volumePhotons, _totalTracedVolumePhotons);
        _volumeTree->buildVolumeHierarchy(true, 1.0f);
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
    _scene = &scene;
    advanceSpp();

    _surfacePhotons.resize(_settings.photonCount);
    if (!_scene->media().empty())
        _volumePhotons.resize(_settings.volumePhotonCount);

    int numThreads = ThreadUtils::pool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        uint32 surfaceRangeStart = intLerp(0, uint32(_surfacePhotons.size()), i + 0, numThreads);
        uint32 surfaceRangeEnd   = intLerp(0, uint32(_surfacePhotons.size()), i + 1, numThreads);
        uint32  volumeRangeStart = intLerp(0, uint32( _volumePhotons.size()), i + 0, numThreads);
        uint32  volumeRangeEnd   = intLerp(0, uint32( _volumePhotons.size()), i + 1, numThreads);
        _taskData.emplace_back(SubTaskData{
            SurfacePhotonRange(&_surfacePhotons[0], surfaceRangeStart, surfaceRangeEnd),
            VolumePhotonRange(_volumePhotons.empty() ? nullptr : &_volumePhotons[0], volumeRangeStart, volumeRangeEnd)
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
