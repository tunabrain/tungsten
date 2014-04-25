#ifndef DISTRIBUTION1D_HPP_
#define DISTRIBUTION1D_HPP_

#include <core/Debug.hpp>
#include "math/MathUtil.hpp"

#include <algorithm>

namespace Tungsten {

class Distribution1D
{
    std::vector<float> _pdf;
    std::vector<float> _cdf;
public:
    Distribution1D(std::vector<float> weights)
    : _pdf(std::move(weights))
    {
        _cdf.resize(_pdf.size() + 1);
        _cdf[0] = 0.0f;
        for (size_t i = 0; i < _pdf.size(); ++i)
            _cdf[i + 1] = _cdf[i] + _pdf[i];
        for (float &p : _pdf)
            p /= _cdf.back();
        for (float &c : _cdf)
            c /= _cdf.back();
        _cdf.back() = 1.0f;
    }

    void warp(float &u, int &idx) const
    {
        idx = std::distance(_cdf.begin(), std::upper_bound(_cdf.begin(), _cdf.end(), u)) - 1;
        u = clamp((u - _cdf[idx])/_pdf[idx], 0.0f, 1.0f);
    }

    float pdf(int idx) const
    {
        return _pdf[idx];
    }
};

}

#endif /* DISTRIBUTION1D_HPP_ */
