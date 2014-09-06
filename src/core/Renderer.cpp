#include "Renderer.hpp"
#include "TraceableScene.hpp"

#include "integrators/Integrator.hpp"

#include "sampling/SobolSampler.hpp"

#include "cameras/Camera.hpp"

#include "extern/lodepng/lodepng.h"

#include <algorithm>

namespace Tungsten {

Renderer::Renderer(const TraceableScene &scene, uint32 threadCount)
: _threadPool(threadCount),
  _sampler(0xBA5EBA11),
  _scene(scene),
  _abortRender(false)
{
    for (uint32 i = 0; i < threadCount; ++i)
        _integrators.emplace_back(scene.cloneThreadSafeIntegrator(i));

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    _varianceW = (_w + VarianceTileSize - 1)/VarianceTileSize;
    _varianceH = (_h + VarianceTileSize - 1)/VarianceTileSize;
    diceTiles();
    _samples.resize(_varianceW*_varianceH);
}

Renderer::~Renderer()
{
    abortRender();
}

void Renderer::diceTiles()
{
    for (uint32 y = 0; y < _h; y += TileSize) {
        for (uint32 x = 0; x < _w; x += TileSize) {
            _tiles.emplace_back(
                x,
                y,
                min(TileSize, _w - x),
                min(TileSize, _h - y),
                _scene.rendererSettings().useSobol() ?
                    std::unique_ptr<SampleGenerator>(new SobolSampler()) :
                    std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
                std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI())))
            );
        }
    }
}

float Renderer::errorPercentile95() const
{
    std::vector<float> errors;
    errors.reserve(_samples.size());

    for (size_t i = 0; i < _samples.size(); ++i) {
        _samples[i].adaptiveWeight = _samples[i].errorEstimate();
        if (_samples[i].adaptiveWeight > 0.0f)
            errors.push_back(_samples[i].adaptiveWeight);
    }
    if (errors.empty())
        return 0.0f;
    std::sort(errors.begin(), errors.end());

    return errors[(errors.size()*95)/100];
}

void Renderer::dilateAdaptiveWeights()
{
    for (uint32 y = 0; y < _varianceH; ++y) {
        for (uint32 x = 0; x < _varianceW; ++x) {
            int idx = x + y*_varianceW;
            if (y < _varianceH - 1)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx + _varianceW].adaptiveWeight);
            if (x < _varianceW - 1)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx + 1].adaptiveWeight);
        }
    }
    for (int y = _varianceH - 1; y >= 0; --y) {
        for (int x = _varianceW - 1; x >= 0; --x) {
            int idx = x + y*_varianceW;
            if (y > 0)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx - _varianceW].adaptiveWeight);
            if (x > 0)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx - 1].adaptiveWeight);
        }
    }
}

void Renderer::distributeAdaptiveSamples(int spp)
{
    double totalWeight = 0.0;
    for (SampleRecord &record : _samples)
        totalWeight += record.adaptiveWeight;

    int adaptiveBudget = (spp - 1)*_w*_h;
    int budgetPerTile = adaptiveBudget/(VarianceTileSize*VarianceTileSize);
    float weightToSampleFactor = double(budgetPerTile)/totalWeight;

    float pixelPdf = 0.0f;
    for (SampleRecord &record : _samples) {
        float fractionalSamples = record.adaptiveWeight*weightToSampleFactor;
        int adaptiveSamples = int(fractionalSamples);
        pixelPdf += fractionalSamples - float(adaptiveSamples);
        if (_sampler.next1D() < pixelPdf) {
            adaptiveSamples++;
            pixelPdf -= 1.0f;
        }
        record.nextSampleCount = adaptiveSamples + 1;
    }
}

bool Renderer::generateWork(uint32 sppFrom, uint32 sppTo)
{
    for (SampleRecord &record : _samples)
        record.sampleIndex += record.nextSampleCount;

    int sppCount = sppTo - sppFrom;
    bool enableAdaptive = _scene.rendererSettings().useAdaptiveSampling();

    if (enableAdaptive && sppFrom >= AdaptiveThreshold) {
        float maxError = errorPercentile95();
        if (maxError == 0.0f)
            return false;

        for (SampleRecord &record : _samples)
            record.adaptiveWeight = min(record.adaptiveWeight, maxError);

        dilateAdaptiveWeights();
        distributeAdaptiveSamples(sppCount);
    } else {
        for (SampleRecord &record : _samples)
            record.nextSampleCount = sppCount;
    }

    return true;
}

void Renderer::renderTile(uint32 id, uint32 tileId)
{
    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;
            uint32 variancePixelIndex = pixel.x()/VarianceTileSize + pixel.y()/VarianceTileSize*_varianceW;

            SampleRecord &record = _samples[variancePixelIndex];
            int spp = record.nextSampleCount;
            Vec3f c(0.0f);
            for (int i = 0; i < spp; ++i) {
                tile.sampler->setup(pixelIndex, record.sampleIndex + i);
                Vec3f s(_integrators[id]->traceSample(pixel, *tile.sampler, *tile.supplementalSampler));

                record.addSample(s);
                c += s;
            }

            _scene.cam().addSamples(x + tile.x, y + tile.y, c, spp);
        }
    }

    if ((--_inFlightCount) == 0 && !_abortRender) {
        _completionCallback();

        std::unique_lock<std::mutex> lock(_completionMutex);
        _completionCond.notify_all();
    }
}

void Renderer::startRender(std::function<void()> completionCallback, uint32 sppFrom, uint32 sppTo)
{
    _completionCallback = std::move(completionCallback);
    _inFlightCount = 0;
    _abortRender = false;
    if (!generateWork(sppFrom, sppTo)) {
        completionCallback();
        return;
    }

    _inFlightCount = _tiles.size();

    using namespace std::placeholders;
    for (uint32 i = 0; i < _tiles.size(); ++i)
        _threadPool.enqueue(std::bind(&Renderer::renderTile, this, _1, i));
}

void Renderer::waitForCompletion()
{
    std::unique_lock<std::mutex> lock(_completionMutex);
    _completionCond.wait(lock, [this]{return _inFlightCount == 0;});
}

void Renderer::abortRender()
{
    _abortRender = true;
    _threadPool.reset();
}

void Renderer::saveVariance(const std::string &path) const
{
    float maxError = max(errorPercentile95(), 1e-5f);
    std::vector<Vec3c> image(_varianceW*_varianceH);
    for (size_t i = 0; i < _samples.size(); ++i)
        image[i] = Vec3c(min(int(_samples[i].errorEstimate()/maxError*256.0f), 255));

    lodepng_encode24_file(path.c_str(), &image[0].x(), _varianceW, _varianceH);
}

}
