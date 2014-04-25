#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include <tbb/concurrent_queue.h>
#include <tbb/parallel_for.h>
#include <thread>
#include <memory>
#include <atomic>

#include "integrators/Integrator.hpp"

#include "sampling/SampleGenerator.hpp"

#include "math/MathUtil.hpp"

#include "TraceableScene.hpp"
#include "IntTypes.hpp"
#include "Config.hpp"
#include "Camera.hpp"

#include "extern/lodepng/lodepng.h"

namespace Tungsten
{

template<typename Integrator>
class Renderer
{
    static constexpr uint32 TileSize = 16;

    struct SampleRecord
    {
        uint32 sampleCount, nextSampleCount;
        float adaptiveWeight;
        float mean, runningVariance;

        SampleRecord()
        : sampleCount(0), mean(0.0f), runningVariance(0.0f)
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

        inline float relativeVariance() const
        {
            return runningVariance/(mean*(sampleCount - 1));
        }
    };

    struct ImageTile
    {
        uint32 x, y, w, h;
        uint32 sppFrom, sppTo;
        std::unique_ptr<SampleGenerator> sampler;
        std::unique_ptr<UniformSampler> supplementalSampler;

        ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
                std::unique_ptr<SampleGenerator> sampler_,
                std::unique_ptr<UniformSampler> supplementalSampler_)
        :   x(x_), y(y_), w(w_), h(h_),
            sppFrom(0), sppTo(0),
            sampler(std::move(sampler_)),
            supplementalSampler(std::move(supplementalSampler_))
        {
        }
    };

    UniformSampler _sampler;
    const TraceableScene &_scene;
    uint32 _w;
    uint32 _h;

    volatile bool _abortRender;

    std::atomic<int> _workerCount;
    std::vector<std::thread> _workerThreads;
    std::vector<SampleRecord> _samples;
    std::vector<ImageTile> _tiles;
    tbb::concurrent_queue<ImageTile *> _tileQueue;

    void diceTiles()
    {
        for (uint32 y = 0; y < _h; y += TileSize) {
            for (uint32 x = 0; x < _w; x += TileSize) {
                _tiles.emplace_back(
                    x,
                    y,
                    min(TileSize, _w - x),
                    min(TileSize, _h - y),
#if USE_SOBOL
                    std::unique_ptr<SampleGenerator>(new SobolSampler()),
#else
                    std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
#endif
                    std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI())))
                );
            }
        }
    }

    void generateWork(uint32 sppFrom, uint32 sppTo)
    {
        if (sppFrom < 16) {
            for (SampleRecord &record : _samples)
                record.nextSampleCount = sppTo - sppFrom;
        } else {
            double sum = 0.0f;
            for (SampleRecord &record : _samples) {
                record.adaptiveWeight = min(record.relativeVariance(), 256.0f);
                sum += record.adaptiveWeight;
            }
            float sampleFactor = double((sppTo - sppFrom - 1)*_w*_h)/sum;
            float pixelPdf = 0.0f;
            for (SampleRecord &record : _samples) {
                float fractionalSamples = record.adaptiveWeight*sampleFactor;
                int adaptiveSamples = int(fractionalSamples);
                pixelPdf += fractionalSamples - float(adaptiveSamples);
                if (_sampler.next1D() < pixelPdf) {
                    adaptiveSamples++;
                    pixelPdf -= 1.0f;
                }
                record.nextSampleCount = adaptiveSamples + 1;
            }
        }

        for (ImageTile &tile : _tiles)
            _tileQueue.push(&tile);
    }

    template<typename PixelCallback, typename FinishCallback>
    void renderTiles(PixelCallback pixelCallback, FinishCallback finishCallback)
    {
        Integrator integrator(_scene);

        _workerCount++;

        ImageTile *tile;
        std::vector<float> sampleDist(TileSize*TileSize);
        while (_tileQueue.try_pop(tile) && !_abortRender) {
            int spp = tile->sppTo - tile->sppFrom;
            int sampleCount = (spp - 1)*tile->w*tile->h;
            float sum = 0.0f;
            for (uint32 y = 0; y < tile->h; ++y) {
                for (uint32 x = 0; x < tile->w; ++x) {
                    if (tile->sppFrom < 16)
                        sampleDist[x + y*TileSize] = 1.0f;
                    else
                        sampleDist[x + y*TileSize] = min(_samples[tile->x + x + (tile->y + y)*_w].relativeVariance(), 256.0f);
                    sum += sampleDist[x + y*TileSize];
                }
            }

            tile->variance = sum*(1.0f/(TileSize*TileSize));

            if (sum == 0.0f)
                continue;

            float pixelPdf = 0.0f;
            for (uint32 y = 0; y < tile->h; ++y) {
                for (uint32 x = 0; x < tile->w; ++x) {
                    Vec2u pixel(tile->x + x, tile->y + y);
                    uint32 pixelIndex = pixel.x() + pixel.y()*_w;

                    float fractionalSamples = (sampleDist[x + y*TileSize]/sum)*sampleCount;
                    int pixelSamples = int(fractionalSamples);
                    pixelPdf += fractionalSamples - float(pixelSamples);
                    if (tile->supplementalSampler->next1D() < pixelPdf) {
                        pixelSamples++;
                        pixelPdf -= 1.0f;
                    }
                    pixelSamples++;

                    pixelSamples = spp;

                    Vec3f c(0.0f);
                    for (uint32 i = 0; i < pixelSamples; ++i) {
                        tile->sampler->setup(pixelIndex, _samples[pixelIndex].sampleCount);
                        Vec3f s(integrator.traceSample(pixel, *tile->sampler, *tile->supplementalSampler));

                        _samples[pixelIndex].addSample(s);
                        c += s;
                    }

                    pixelCallback(x + tile->x, y + tile->y, c, pixelSamples);
                }
            }
        }

        if (--_workerCount == 0 && !_abortRender)
            finishCallback();
    }

public:
    Renderer(const TraceableScene &scene)
    : _sampler(0xBA5EBA11), _scene(scene), _abortRender(false)
    {
        _w = scene.cam().resolution().x();
        _h = scene.cam().resolution().y();
        diceTiles();
        _samples.resize(_w*_h);
    }

    ~Renderer()
    {
        abortRender();
    }

    template<typename PixelCallback, typename FinishCallback>
    void startRender(PixelCallback pixelCallback, FinishCallback finishCallback, uint32 sppFrom, uint32 sppTo, uint32 threadCount)
    {
        _workerCount = 0;
        _abortRender = false;
        generateWork(sppFrom, sppTo);

        for (uint32 i = 0; i < threadCount; ++i)
            _workerThreads.emplace_back(&Renderer::renderTiles<PixelCallback, FinishCallback>, this, pixelCallback, finishCallback);
    }

    void waitForCompletion()
    {
        for (std::thread &t : _workerThreads)
            t.join();
        _workerThreads.clear();
    }

    void abortRender()
    {
        _abortRender = true;
        ImageTile *tile;
        while (_tileQueue.try_pop(tile));
        waitForCompletion();
    }

    void saveVariance(const std::string &path) const
    {
        std::vector<uint8> image(_w*_h*3);
        for (size_t i = 0; i < _samples.size(); ++i)
            image[i*3] = image[i*3 + 1] = image[i*3 + 2] =
                min(int(_samples[i].relativeVariance()*256.0f), 255);

        lodepng_encode24_file(path.c_str(), &image[0], _w, _h);
    }
};

}

#endif /* RENDERER_HPP_ */
