#include "BidirectionalPathTraceIntegrator.hpp"
#include "ImagePyramid.hpp"

#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

namespace Tungsten {

CONSTEXPR uint32 BidirectionalPathTraceIntegrator::TileSize;

BidirectionalPathTraceIntegrator::BidirectionalPathTraceIntegrator()
: Integrator(),
  _w(0),
  _h(0),
  _sampler(0xBA5EBA11)
{
}

BidirectionalPathTraceIntegrator::~BidirectionalPathTraceIntegrator()
{
}

void BidirectionalPathTraceIntegrator::diceTiles()
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

void BidirectionalPathTraceIntegrator::renderTile(uint32 id, uint32 tileId)
{
    int spp = _nextSpp - _currentSpp;

    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;

            for (int i = 0; i < spp; ++i) {
                uint32 lightPathId = pixelIndex*_scene->rendererSettings().spp() + _currentSpp + i;
                tile.sampler->startPath(pixelIndex, _currentSpp + i);
                Vec3f c = _tracers[id]->traceSample(pixel, lightPathId, *tile.sampler);

                _scene->cam().colorBuffer()->addSample(pixel, c);
            }
        }
    }
}

void BidirectionalPathTraceIntegrator::saveState(OutputStreamHandle &out)
{
    for (ImageTile &i : _tiles)
        i.sampler->saveState(out);
}

void BidirectionalPathTraceIntegrator::loadState(InputStreamHandle &in)
{
    for (ImageTile &i : _tiles)
        i.sampler->loadState(in);
}

void BidirectionalPathTraceIntegrator::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    _settings.fromJson(value);
}

rapidjson::Value BidirectionalPathTraceIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void BidirectionalPathTraceIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _currentSpp = 0;
    _sampler = UniformSampler(MathUtil::hash32(seed));
    _scene = &scene;
    advanceSpp();

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    scene.cam().requestColorBuffer();
    scene.cam().requestSplatBuffer();

    if (_settings.imagePyramid)
        _imagePyramid.reset(new ImagePyramid(_settings.maxBounces, _scene->cam()));

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i)
        _tracers.emplace_back(new BidirectionalPathTracer(&scene, _settings, i, _imagePyramid.get()));

    diceTiles();
}

void BidirectionalPathTraceIntegrator::teardownAfterRender()
{
    _group.reset();

    _tracers.clear();
    _tiles  .clear();
    _tracers.shrink_to_fit();
    _tiles  .shrink_to_fit();
}

bool BidirectionalPathTraceIntegrator::supportsResumeRender() const
{
    return true;
}

void BidirectionalPathTraceIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    _scene->cam().setSplatWeight(1.0/(_w*_h*_nextSpp));

    using namespace std::placeholders;
    _group = ThreadUtils::pool->enqueue(
        std::bind(&BidirectionalPathTraceIntegrator::renderTile, this, _3, _1),
        _tiles.size(),
        [&, completionCallback]() {
            _currentSpp = _nextSpp;
            advanceSpp();
            completionCallback();
        }
    );
}

void BidirectionalPathTraceIntegrator::waitForCompletion()
{
    if (_group) {
        _group->wait();
        _group.reset();
    }
}

void BidirectionalPathTraceIntegrator::abortRender()
{
    if (_group) {
        _group->abort();
        _group->wait();
        _group.reset();
    }
}

void BidirectionalPathTraceIntegrator::saveOutputs()
{
    Integrator::saveOutputs();
    if (_settings.imagePyramid) {
        Path pathPrefix = _scene->rendererSettings().outputFile().stripExtension();
        _imagePyramid->saveBuffers(pathPrefix, _scene->rendererSettings().spp(), false);
    }
}

}
