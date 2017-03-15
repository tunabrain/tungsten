#include "NlMeans.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "Logging.hpp"

#include <Eigen/Dense>
#include <iostream>
#include <vector>

namespace Tungsten {

Pixmap3f collaborativeRegression(const Pixmap3f &image, const Pixmap3f &guide,
        const std::vector<PixmapF> &features, const Pixmap3f &imageVariance,
        int F, int R, float k)
{
    int w = image.w();
    int h = image.h();
    int d = features.size() + 3;

    // We parallellize by dicing up the image into 32x32 tiles
    const int TileSize = 32;
    int padSize = TileSize + 2*F;

    struct Tile
    {
        Vec2i pos;
        Box2i dstRect;
        Pixmap3f result;
        PixmapF resultWeights;
        Tile(int x, int y) : pos(x, y) {}
    };

    std::vector<Tile> tiles;
    for (int tileY : range(0, h, TileSize))
        for (int tileX : range(0, w, TileSize))
            tiles.emplace_back(tileX, tileY);

    struct PerThreadData { Pixmap3f tmpBufA, tmpBufB; std::vector<PixmapF> weights; };
    std::vector<std::unique_ptr<PerThreadData>> threadData(ThreadUtils::idealThreadCount());

    ThreadUtils::pool->enqueue([&](uint32 i, uint32, uint32 threadId) {
        printProgressBar(i, tiles.size());

        if (!threadData[threadId]) {
            threadData[threadId].reset(new PerThreadData());
            threadData[threadId]->tmpBufA = Pixmap3f(padSize, padSize);
            threadData[threadId]->tmpBufB = Pixmap3f(padSize, padSize);
            for (int i = 0; i < (2*R + 1)*(2*R + 1); ++i)
                threadData[threadId]->weights.emplace_back(TileSize, TileSize);
        }
        auto &data = *threadData[threadId];
        Tile &tile = tiles[i];

        for (auto &w : data.weights)
            w.clear();

        Box2i srcRect(tile.pos, min(tile.pos + TileSize, Vec2i(w, h)));
        tile.dstRect = srcRect;
        tile.dstRect.grow(R);
        tile.dstRect.intersect(Box2i(Vec2i(0), Vec2i(w, h)));
        int dstW = tile.dstRect.diagonal().x(), dstH = tile.dstRect.diagonal().y();

        tile.result = Pixmap3f(dstW, dstH);
        tile.resultWeights = PixmapF(dstW, dstH);

        // Precompute weights for entire tile
        for (int dy = -R, idxW = 0; dy <= R; ++dy)
            for (int dx = -R; dx <= R; ++dx, ++idxW)
                nlMeansWeights(data.weights[idxW], data.tmpBufA, data.tmpBufB, guide, imageVariance,
                        srcRect, F, k, dx, dy, 2.0f);

        for (int y = srcRect.min().y(); y < srcRect.max().y(); ++y) {
            for (int x = srcRect.min().x(); x < srcRect.max().x(); ++x) {
                int x0 = max(x - R, 0), x1 = min(w, x + R + 1);
                int y0 = max(y - R, 0), y1 = min(h, y + R + 1);
                int n = (x1 - x0)*(y1 - y0);

                // Build weight matrix (W), feature matrix (X) and RHS (Y)
                Eigen::VectorXf W(n);
                Eigen::MatrixXf Y(n, 3);
                Eigen::MatrixXf X(n, d);

                for (int iy = y0; iy < y1; ++iy) {
                    for (int ix = x0; ix < x1; ++ix) {
                        int idxP = ix + iy*w;
                        int idx = (ix - x0) + (iy - y0)*(x1 - x0);

                        for (int i = 0; i < 3; ++i)
                            Y(idx, i) = image[idxP][i];

                        X(idx, 0) = 1.0f;
                        X(idx, 1) = ix - x;
                        X(idx, 2) = iy - y;
                        for (size_t i = 0; i < features.size(); ++i)
                            X(idx, i + 3) = features[i][idxP] - features[i][x + y*w];

                        int idxW = (ix - x + R) + (iy - y + R)*(2*R + 1);
                        W[idx] = data.weights[idxW][Vec2i(x, y) - tile.pos];
                    }
                }

                // Solve least squares system
                Eigen::VectorXf wSqrt = W.cwiseSqrt();
                Eigen::MatrixXf denoised = X*(wSqrt.asDiagonal()*X).colPivHouseholderQr().solve(wSqrt.asDiagonal()*Y);

                // Accumulate denoised patch into image
                for (int iy = y0; iy < y1; ++iy) {
                    for (int ix = x0; ix < x1; ++ix) {
                        Vec2i p = Vec2i(ix, iy) - tile.dstRect.min();
                        int idx = (ix - x0) + (iy - y0)*(x1 - x0);
                        tile.result       [p] += W[idx]*Vec3f(denoised(idx, 0), denoised(idx, 1), denoised(idx, 2));
                        tile.resultWeights[p] += W[idx];
                    }
                }
            }
        }

    }, tiles.size())->wait();

    // Gather results from all threads and divide by weights
    Pixmap3f result(w, h);
    PixmapF resultWeights(w, h);
    for (const auto &tile : tiles) {
        for (int y  : tile.dstRect.range(1)) {
            for (int x  : tile.dstRect.range(0)) {
                Vec2i p(x, y);
                result       [p] += tile.result       [p - tile.dstRect.min()];
                resultWeights[p] += tile.resultWeights[p - tile.dstRect.min()];
            }
        }
    }
    for (int j = 0; j < w*h; ++j)
        result[j] /= resultWeights[j];

    printProgressBar(tiles.size(), tiles.size());

    return std::move(result);
}

}
