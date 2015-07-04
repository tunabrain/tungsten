#ifndef INTERPOLATEDDISTRIBUTION1D_HPP_
#define INTERPOLATEDDISTRIBUTION1D_HPP_

#include "math/MathUtil.hpp"

#include <vector>

namespace Tungsten {

// This class can sample from a procedural 1D distribution that is interpolated from
// two or more discrete distributions.
class InterpolatedDistribution1D
{
    int _size;
    int _numDistributions;

    std::vector<float> _pdfs;
    std::vector<float> _cdfs;
    std::vector<float> _sums;

    float cdfs(int x, int distribution) const
    {
        return _cdfs[x + distribution*(_size + 1)];
    }

    float pdfs(int x, int distribution) const
    {
        return _pdfs[x + distribution*_size];
    }

    float &cdfs(int x, int distribution)
    {
        return _cdfs[x + distribution*(_size + 1)];
    }

    float &pdfs(int x, int distribution)
    {
        return _pdfs[x + distribution*_size];
    }

public:
    InterpolatedDistribution1D(std::vector<float> weights, int size, int numDistributions)
    : _size(size),
      _numDistributions(numDistributions),
      _pdfs(std::move(weights)),
      _cdfs((size + 1)*numDistributions),
      _sums(numDistributions)
    {
        for (int dist = 0; dist < _numDistributions; ++dist) {
            cdfs(0, dist) = 0.0f;
            for (int x = 0; x < _size; ++x)
                cdfs(x + 1, dist) = pdfs(x, dist) + cdfs(x, dist);

            _sums[dist] = cdfs(_size, dist);

            if (_sums[dist] < 1e-4f) {
                // Revert to uniform sampling for near-degenerate distributions
                float ratio = 1.0f/_size;
                for (int x = 0; x < _size; ++x) {
                    pdfs(x, dist) = ratio;
                    cdfs(x, dist) = x*ratio;
                }
            } else {
                float scale = 1.0f/_sums[dist];
                for (int x = 0; x < _size; ++x) {
                    pdfs(x, dist) *= scale;
                    cdfs(x, dist) *= scale;
                }
            }
            cdfs(_size, dist) = 1.0f;
        }
    }

    void warp(float distribution, float &u, int &x) const
    {
        int d0 = clamp(int(distribution), 0, _numDistributions - 1);
        int d1 = min(d0 + 1, _numDistributions - 1);
        float v = clamp(distribution - d0, 0.0f, 1.0f);

        int lower = 0, upper = _size;
        float lowerU = 0.0f, upperU = 1.0f;

        while (upper - lower != 1) {
            int midpoint = (upper + lower)/2;
            float midpointU = cdfs(midpoint, d0)*(1.0f - v) + cdfs(midpoint, d1)*v;
            if (midpointU < u) {
                lower = midpoint;
                lowerU = midpointU;
            } else {
                upper = midpoint;
                upperU = midpointU;
            }
        }

        x = lower;
        u = clamp((u - lowerU)/(upperU - lowerU), 0.0f, 1.0f);
    }

    float pdf(float distribution, int x) const
    {
        int d0 = clamp(int(distribution), 0, _numDistributions - 1);
        int d1 = min(d0 + 1, _numDistributions - 1);
        float v = clamp(distribution - d0, 0.0f, 1.0f);

        return pdfs(x, d0)*(1.0f - v) + pdfs(x, d1)*v;
    }

    float sum(float distribution) const
    {
        int d0 = clamp(int(distribution), 0, _numDistributions - 1);
        int d1 = min(d0 + 1, _numDistributions - 1);
        float v = clamp(distribution - d0, 0.0f, 1.0f);

        return _sums[d0]*(1.0f - v) + _sums[d1]*v;
    }
};

}

#endif /* INTERPOLATEDDISTRIBUTION1D_HPP_ */
