#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include <functional>
#include <algorithm>
#include <thread>
#include <memory>
#include <atomic>

#include "integrators/Integrator.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"
#include "sampling/SobolSampler.hpp"

#include "cameras/Camera.hpp"

#include "math/MathUtil.hpp"

#include "TraceableScene.hpp"
#include "ThreadPool.hpp"
#include "IntTypes.hpp"

#include "extern/lodepng/lodepng.h"

namespace Tungsten {

struct SampleRecord
{
    uint32 sampleCount, nextSampleCount, sampleIndex;
    float adaptiveWeight;
    float mean, runningVariance;

    SampleRecord()
    : sampleCount(0), nextSampleCount(0), sampleIndex(0),
      adaptiveWeight(0.0f),
      mean(0.0f), runningVariance(0.0f)
    {
    }

    inline void addSample(float x)
    {
        sampleCount++;
        float delta = x - mean;
        mean += delta/sampleCount;
        runningVariance += delta*(x - mean);
    }

    inline void addSample(const Vec3f &x)
    {
        addSample(x.luminance());
    }

    inline float variance() const
    {
        return runningVariance/(sampleCount - 1);
    }

    inline float errorEstimate() const
    {
        return variance()/(sampleCount*max(sqr(mean), 1e-3f));
    }
};

struct ImageTile
{
    uint32 x, y, w, h;
    std::unique_ptr<SampleGenerator> sampler;
    std::unique_ptr<UniformSampler> supplementalSampler;

    ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
            std::unique_ptr<SampleGenerator> sampler_,
            std::unique_ptr<UniformSampler> supplementalSampler_)
    :   x(x_), y(y_), w(w_), h(h_),
        sampler(std::move(sampler_)),
        supplementalSampler(std::move(supplementalSampler_))
    {
    }
};

class Renderer
{
    static constexpr uint32 TileSize = 16;
    static constexpr uint32 VarianceTileSize = 4;
    static constexpr uint32 AdaptiveThreshold = 16;

    ThreadPool _threadPool;

    std::atomic<bool> _abortRender;
    std::atomic<int> _inFlightCount;

    std::mutex _completionMutex;
    std::condition_variable _completionCond;
    std::function<void()> _completionCallback;

    uint32 _w;
    uint32 _h;
    uint32 _varianceW;
    uint32 _varianceH;

    UniformSampler _sampler;
    const TraceableScene &_scene;
    std::vector<std::unique_ptr<Integrator>> _integrators;

    std::vector<SampleRecord> _samples;
    std::vector<ImageTile> _tiles;

    void diceTiles()
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

    void dilateAdaptiveWeights()
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

    float errorPercentile95() const
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

    void distributeAdaptiveSamples(int spp)
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

    bool generateWork(uint32 sppFrom, uint32 sppTo)
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

    void renderTile(uint32 id, uint32 tileId)
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

public:
    Renderer(const TraceableScene &scene, uint32 threadCount)
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

    ~Renderer()
    {
        abortRender();
    }

    void startRender(std::function<void()> completionCallback, uint32 sppFrom, uint32 sppTo)
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

    void waitForCompletion()
    {
        std::unique_lock<std::mutex> lock(_completionMutex);
        _completionCond.wait(lock, [this]{return _inFlightCount == 0;});
    }

    void abortRender()
    {
        _abortRender = true;
        _threadPool.reset();
    }

    void saveVariance(const std::string &path) const
    {
        float maxError = max(errorPercentile95(), 1e-5f);
        std::vector<Vec3c> image(_varianceW*_varianceH);
        for (size_t i = 0; i < _samples.size(); ++i)
            image[i] = Vec3c(min(int(_samples[i].errorEstimate()/maxError*256.0f), 255));

        lodepng_encode24_file(path.c_str(), &image[0].x(), _varianceW, _varianceH);
    }
};

}

#endif /* RENDERER_HPP_ */
