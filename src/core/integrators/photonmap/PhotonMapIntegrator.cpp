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
  _currentPhotonCount(0),
  _nextPhotonCount(0),
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

void PhotonMapIntegrator::advancePhotonCount()
{
    _nextPhotonCount = min(_currentPhotonCount + _settings.photonsPerIteration, _settings.photonCount);
}

void PhotonMapIntegrator::saveState(OutputStreamHandle &/*out*/)
{
}

void PhotonMapIntegrator::loadState(InputStreamHandle &/*in*/)
{
}

void PhotonMapIntegrator::tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId)
{
    uint32 photonStart = intLerp(_currentPhotonCount, _nextPhotonCount, taskId + 0, numSubTasks);
    uint32 photonEnd   = intLerp(_currentPhotonCount, _nextPhotonCount, taskId + 1, numSubTasks);

    SubTaskData &data = _taskData[taskId];

    uint32 totalCast = 0;
    while (photonStart < photonEnd) {
        data.sampler->setup(photonStart, 0);
        int traceCount = _tracers[threadId]->tracePhoton(
            &_photons[photonStart + _photonOffset],
            photonEnd - photonStart,
            *data.sampler,
            *data.supplementalSampler
        );
        totalCast++;
        photonStart += traceCount;
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
    advancePhotonCount();

    _photonOffset = KdTree::computePadding(_settings.photonCount);
    _photons.resize(_photonOffset + _settings.photonCount);

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i) {
        _taskData.emplace_back(SubTaskData{
            _scene->rendererSettings().useSobol() ?
                std::unique_ptr<SampleGenerator>(new SobolSampler()) :
                std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
            std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI())))
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
    if (_currentPhotonCount < _nextPhotonCount) {
        _group = ThreadUtils::pool->enqueue(
            std::bind(&PhotonMapIntegrator::tracePhotons, this, _1, _2, _3),
            _tracers.size(),
            [&, completionCallback]() {
                _currentPhotonCount = _nextPhotonCount;
                advancePhotonCount();
                completionCallback();
            }
        );
    } else {
        if (!_tree) {
            float scale = 1.0f/_totalTracedPhotons;
            for (uint32 i = 0; i < _settings.photonCount; ++i)
                _photons[_photonOffset + i].power *= scale;

            _tree.reset(new KdTree(std::move(_photons), _photonOffset));
        }

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
