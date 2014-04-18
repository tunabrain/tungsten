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

#include "IntTypes.hpp"
#include "Config.hpp"

namespace Tungsten
{

template<typename Integrator>
class Renderer
{
    static constexpr uint32 TileSize = 16;

    struct ImageTile
    {
        uint32 x, y, w, h;
        uint32 seed;
        uint32 sppFrom, sppTo;
    };

    const Camera &_cam;
    const PackedGeometry &_mesh;
    const std::vector<std::shared_ptr<Light>> &_lights;
    uint32 _w;
    uint32 _h;

    volatile bool _abortRender;

    std::atomic<int> _workerCount;
    std::vector<std::thread> _workerThreads;
    tbb::concurrent_queue<ImageTile> _tiles;

    void diceTiles(uint32 sppFrom, uint32 sppTo)
    {
        UniformSampler sampler(0xBA5EBA11, 0);

        for (uint32 y = 0; y < _h; y += TileSize) {
            for (uint32 x = 0; x < _w; x += TileSize) {
                _tiles.push(ImageTile{
                    x,
                    y,
                    min(TileSize, _w - x),
                    min(TileSize, _h - y),
                    MathUtil::hash32(sampler.iSample(0)),
                    sppFrom,
                    sppTo
                });
            }
        }
    }

    template<typename PixelCallback, typename FinishCallback>
    void renderTiles(PixelCallback pixelCallback, FinishCallback finishCallback)
    {
        Integrator integrator(_mesh, _lights);

        _workerCount++;

        ImageTile tile;
        while (_tiles.try_pop(tile) && !_abortRender) {
            UniformSampler perPixelGenerator(tile.seed, 0);

            for (uint32 y = 0; y < tile.h; ++y) {
                for (uint32 x = 0; x < tile.w; ++x) {
                    Vec3f c;

                    uint32 pixelSeed = perPixelGenerator.iSample(0);

                    for (uint32 i = tile.sppFrom; i < tile.sppTo; ++i) {
#if USE_SOBOL
                        SampleGenerator sampler(pixelSeed, i);
#else
                        SampleGenerator &sampler = perPixelGenerator;
#endif
                        c += integrator.traceSample(_cam, Vec2u(tile.x + x, tile.y + y), sampler);
                    }

                    pixelCallback(x + tile.x, y + tile.y, c, tile.sppTo - tile.sppFrom);
                }
            }
        }

        if (--_workerCount == 0 && !_abortRender)
            finishCallback();
    }

public:
    Renderer(const Camera &cam,
             const PackedGeometry &mesh,
             const std::vector<std::shared_ptr<Light>> &lights)
    : _cam(cam), _mesh(mesh), _lights(lights), _w(cam.resolution().x()), _h(cam.resolution().y()), _abortRender(false)
    {
    }

    template<typename PixelCallback, typename FinishCallback>
    void startRender(PixelCallback pixelCallback, FinishCallback finishCallback, uint32 sppFrom, uint32 sppTo, uint32 threadCount)
    {
        for (const std::shared_ptr<Light> &l : _lights)
            l->prepareForRender();

        _workerCount = 0;
        _abortRender = false;
        diceTiles(sppFrom, sppTo);

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
        ImageTile tile;
        while (_tiles.try_pop(tile));
        waitForCompletion();
    }
};

}

#endif /* RENDERER_HPP_ */
