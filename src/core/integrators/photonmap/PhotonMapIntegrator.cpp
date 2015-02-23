#include "PhotonMapIntegrator.hpp"
#include "PhotonTracer.hpp"

#include "sampling/SobolSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

namespace Tungsten {

CONSTEXPR uint32 PhotonMapIntegrator::TileSize;

PhotonMapIntegrator::PhotonMapIntegrator()
: _w(0),
  _h(0),
  _sampler(0xBA5EBA11),
  _photonOffset(0)
{
}

static int intLerp(int x0, int x1, int t, int range)
{
    return (x0*(range - t) + x1*t)/range;
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
                    std::unique_ptr<SampleGenerator>(new SobolSampler()) :
                    std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
                std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI())))
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

    uint32 photonBase    = intLerp(0, _settings.photonCount, taskId + 0, numSubTasks);
    uint32 photonsToCast = intLerp(0, _settings.photonCount, taskId + 1, numSubTasks) - photonBase;

    uint32 totalCast;
    for (totalCast = 0; totalCast < photonsToCast && data.photonNext < data.photonEnd; ++totalCast) {
        data.sampler->setup(taskId, photonBase + totalCast);
        data.photonNext += _tracers[threadId]->tracePhoton(
            &_photons[data.photonNext],
            data.photonEnd - data.photonNext,
            *data.sampler,
            *data.supplementalSampler
        );
    }

    _totalTracedPhotons += totalCast;
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
                tile.sampler->setup(pixelIndex, _currentSpp + i);
                Vec3f s(_tracers[threadId]->traceSample(pixel, *_tree, *tile.sampler, *tile.supplementalSampler));
                c += s;
            }

            _scene->cam().addSamples(x + tile.x, y + tile.y, c, spp);
        }
    }
}

void PhotonMapIntegrator::buildPhotonDataStructures()
{
    uint32 tail = 0;
    for (size_t i = 0; i < _taskData.size(); ++i) {
        uint32 gap = _taskData[i].photonEnd - _taskData[i].photonNext;

        for (size_t t = i + 1; t < _taskData.size() && gap > 0; ++t) {
            uint32 copyCount = min(gap, _taskData[t].photonNext - _taskData[t].photonStart);
            if (copyCount > 0) {
                std::memcpy(
                        &_photons[_taskData[i].photonNext],
                        &_photons[_taskData[t].photonNext - copyCount],
                        copyCount*sizeof(Photon)
                );
            }
            _taskData[i].photonNext += copyCount;
            _taskData[t].photonNext -= copyCount;
            gap -= copyCount;
        }

        tail = _taskData[i].photonNext;
        if (gap > 0)
            break;
    }

    float scale = 1.0f/_totalTracedPhotons;
    for (uint32 i = 0; i < _settings.photonCount; ++i)
        _photons[_photonOffset + i].power *= scale;

    _tree.reset(new KdTree(std::move(_photons), _photonOffset, tail));
}

void PhotonMapIntegrator::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    _settings.fromJson(v);
}

rapidjson::Value PhotonMapIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void PhotonMapIntegrator::prepareForRender(TraceableScene &scene)
{
    _sampler = UniformSampler(0xBA5EBA11);
    _totalTracedPhotons = 0;
    _scene = &scene;
    advanceSpp();

    _photonOffset = KdTree::computePadding(_settings.photonCount);
    _photons.resize(_photonOffset + _settings.photonCount);

    int numThreads = ThreadUtils::pool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        uint32 rangeStart = intLerp(_photonOffset, uint32(_photons.size()), i + 0, numThreads);
        uint32 rangeEnd   = intLerp(_photonOffset, uint32(_photons.size()), i + 1, numThreads);
        _taskData.emplace_back(SubTaskData{
            _scene->rendererSettings().useSobol() ?
                std::unique_ptr<SampleGenerator>(new SobolSampler()) :
                std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
            std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
            rangeStart,
            rangeStart,
            rangeEnd
        });

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

    _tree.reset();
    _tracers .clear();
    _photons .clear();
    _taskData.clear();
    _tracers .shrink_to_fit();
    _photons .shrink_to_fit();
    _taskData.shrink_to_fit();
}

void PhotonMapIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    using namespace std::placeholders;
    if (!_tree) {
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
