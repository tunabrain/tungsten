#ifndef NLMEANS_HPP_
#define NLMEANS_HPP_

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "sse/SimdFloat.hpp"

#include "BoxFilter.hpp"
#include "Pixmap.hpp"

#include "Logging.hpp"

#include <tinyformat/tinyformat.hpp>
#include <fmath/fmath.hpp>

namespace Tungsten {

static inline float fastExp(float f)
{
    return float4(fmath::exp_ps(float4(f).raw()))[0];
}
static inline Vec3f fastExp(Vec3f f)
{
    float4 expF = fmath::exp_ps(float4(f.x(), f.y(), f.z(), 0.0f).raw());
    return Vec3f(expF[0], expF[1], expF[2]);
}
static inline float4 fastExp(float4 f)
{
    return fmath::exp_ps(f.raw());
}
template<typename T>
inline void convertWeight(T &out, const T &in)
{
    out = in;
}
static inline void convertWeight(float &out, const Vec3f &in)
{
    out = in.min();
}

// There is a substantial amount of shared computation when evaluating
// the NL-means weights of adjancent pixels. This function computes NL-Means
// weights of an entire rectangle (srcRect) and reuses computation where
// possible
template<typename WeightTexel, typename Texel>
void nlMeansWeights(Pixmap<WeightTexel> &weights, Pixmap<Texel> &distances, Pixmap<Texel> &tmp,
        const Pixmap<Texel> &guide, const Pixmap<Texel> &variance,
        Box2i srcRect, int F, float k, int dx, int dy, const float varianceScale = 1.0f)
{
    const float Epsilon = 1e-7f;
    const float MinCenterWeight = 1e-4f;
    const float DistanceClamp = 10000.0f;

    Box2i imageRect(Vec2i(0), Vec2i(guide.w(), guide.h()));
    Vec2i delta = Vec2i(dx, dy);

    Box2i clippedSrc(srcRect.min() + delta, srcRect.max() + delta);
    clippedSrc.intersect(imageRect);
    clippedSrc = Box2i(clippedSrc.min() - delta, clippedSrc.max() - delta);

    Box2i paddedClippedSrc = srcRect;
    paddedClippedSrc.grow(F);
    paddedClippedSrc.intersect(imageRect);
    paddedClippedSrc = Box2i(paddedClippedSrc.min() + delta, paddedClippedSrc.max() + delta);
    paddedClippedSrc.intersect(imageRect);
    paddedClippedSrc = Box2i(paddedClippedSrc.min() - delta, paddedClippedSrc.max() - delta);

    // From Rousselle et al.'s paper
    auto squaredDist = [&](Vec2i p) {
        Vec2i q = p + delta;
        Texel varP = variance[p]*varianceScale;
        Texel varQ = variance[q]*varianceScale;
        Texel squaredDiff = sqr(guide[p] - guide[q]) - (varP + min(varP, varQ));
        Texel dist = squaredDiff/((varP + varQ)*k*k + Epsilon);
        return min(dist, Texel(DistanceClamp));
    };

    for (int y : paddedClippedSrc.range(1))
        for (int x : paddedClippedSrc.range(0))
            distances[Vec2i(x, y) - paddedClippedSrc.min()] = squaredDist(Vec2i(x, y));

    boxFilter(distances, tmp, distances, F, Box2i(Vec2i(0), paddedClippedSrc.diagonal()));

    for (int y : clippedSrc.range(1))
        for (int x : clippedSrc.range(0))
            convertWeight(weights[Vec2i(x, y) - srcRect.min()], fastExp(-max(distances[Vec2i(x, y) - paddedClippedSrc.min()], Texel(0.0f))));

    if (dx == 0 && dy == 0)
        for (int y : clippedSrc.range(1))
            for (int x : clippedSrc.range(0))
                weights[Vec2i(x, y) - srcRect.min()] = max(weights[Vec2i(x, y) - srcRect.min()], WeightTexel(MinCenterWeight));
}

template<typename Texel>
Pixmap<Texel> nlMeans(const Pixmap<Texel> &image, const Pixmap<Texel> &guide, const Pixmap<Texel> &variance,
        int F, int R, float k, const float varianceScale = 1.0f, bool printProgress = false)
{
    int w = image.w();
    int h = image.h();

    // We parallelize by dicing the input image up into 32x32 tiles
    const int TileSize = 32;
    int padSize = TileSize + 2*F;

    std::vector<Vec2i> tiles;
    for (int tileY : range(0, h, TileSize))
        for (int tileX : range(0, w, TileSize))
            tiles.emplace_back(tileX, tileY);

    struct PerThreadData { Pixmap<Texel> weights, tmpBufA, tmpBufB; };
    std::vector<std::unique_ptr<PerThreadData>> threadData(ThreadUtils::idealThreadCount());

    Pixmap<Texel> result(w, h), resultWeights(w, h);

    ThreadUtils::pool->enqueue([&](uint32 i, uint32, uint32 threadId) {
        if (printProgress)
            printProgressBar(i, tiles.size());

        if (!threadData[threadId]) {
            threadData[threadId].reset(new PerThreadData {
                Pixmap<Texel>(TileSize, TileSize),
                Pixmap<Texel>(padSize, padSize),
                Pixmap<Texel>(padSize, padSize)
            });
        }
        auto &data = *threadData[threadId];

        Vec2i tile = tiles[i];
        Box2i tileRect(tile, min(tile + TileSize, Vec2i(w, h)));

        for (int dy = -R; dy <= R; ++dy) {
            for (int dx = -R; dx <= R; ++dx) {
                Box2i shiftedRect(Vec2i(-dx, -dy), Vec2i(w - dx, h - dy));
                shiftedRect.intersect(tileRect);

                nlMeansWeights(data.weights, data.tmpBufA, data.tmpBufB, guide, variance, shiftedRect, F, k, dx, dy, varianceScale);

                for (int y : shiftedRect.range(1)) {
                    for (int x : shiftedRect.range(0)) {
                        Vec2i p(x, y);
                        Texel weight = data.weights[p - shiftedRect.min()];
                        result       [p] += weight*image[p + Vec2i(dx, dy)];
                        resultWeights[p] += weight;
                    }
                }
            }
        }
    }, tiles.size())->wait();

    for (int j = 0; j < w*h; ++j)
        result[j] /= resultWeights[j];
    if (printProgress)
        printProgressBar(tiles.size(), tiles.size());

    return std::move(result);
}

// This class gathers up 1-channel images and denoises them 4 of them
// simultaneously by packing 4 images into one SIMD float. This is
// useful when denoising feature buffers and yields a ~2x speedup compared
// to filtering each 1-channel image separately.
class SimdNlMeans
{
    struct NlMeansParams
    {
        PixmapF &dst;
        const PixmapF &image, &guide, &variance;
    };

    std::vector<NlMeansParams> _params;

public:
    void addBuffer(PixmapF &dst, const PixmapF &image, const PixmapF &guide, const PixmapF &variance)
    {
        _params.emplace_back(NlMeansParams{dst, image, guide, variance});
    }

    void denoise(int F, int R, float k, float varianceScale = 1.0f)
    {
        if (_params.empty())
            return;
        int w = _params[0].image.w();
        int h = _params[0].image.h();

        Pixmap4pf image(w, h), guide(w, h), variance(w, h);

        int numBlocks = (_params.size() + 3)/4;
        for (size_t i = 0; i < _params.size(); i += 4) {
            int lim = min(i + 4, _params.size());

            for (int k = i; k < lim; ++k) {
                int idx = k % 4;
                for (int j = 0; j < w*h; ++j) {
                    image   [j][idx] = _params[k].image   [j];
                    guide   [j][idx] = _params[k].guide   [j];
                    variance[j][idx] = _params[k].variance[j];
                }
            }

            printTimestampedLog(tfm::format("Denoising feature set %d/%d", i/4 + 1, numBlocks));
            image = nlMeans(image, guide, variance, F, R, k, varianceScale, true);

            for (int k = i; k < lim; ++k) {
                _params[k].dst = PixmapF(w, h);
                for (int j = 0; j < w*h; ++j)
                    _params[k].dst[j] = image[j][k % 4];
            }
        }

        _params.clear();
    }
};

}

#endif /* NLMEANS_HPP_ */
