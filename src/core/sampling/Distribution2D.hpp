#ifndef DISTRIBUTION2D_HPP_
#define DISTRIBUTION2D_HPP_

#include "math/MathUtil.hpp"

#include <algorithm>
#include <vector>

namespace Tungsten {

class Distribution2D
{
    int _w, _h;
    std::vector<float> _marginalPdf, _marginalCdf;
    std::vector<float> _pdf;
    std::vector<float> _cdf;
public:
    Distribution2D(std::vector<float> weights, int w, int h)
    : _w(w), _h(h), _pdf(std::move(weights))
    {
        _cdf.resize(_pdf.size() + h);
        _marginalPdf.resize(h, 0.0f);
        _marginalCdf.resize(h + 1);

        _marginalCdf[0] = 0.0f;
        for (int y = 0; y < h; ++y) {
            int idxP = y*w;
            int idxC = y*(w + 1);

            _cdf[idxC] = 0.0f;
            for (int x = 0; x < w; ++x, ++idxP, ++idxC) {
                _marginalPdf[y] += _pdf[idxP];
                _cdf[idxC + 1] = _cdf[idxC] + _pdf[idxP];
            }
            _marginalCdf[y + 1] = _marginalCdf[y] + _marginalPdf[y];
        }

        for (int y = 0; y < h; ++y) {
            int idxP = y*w;
            int idxC = y*(w + 1);
            int idxTail = idxC + w;

            float rowWeight = _cdf[idxTail];
            if (rowWeight < 1e-4f) {
                for (int x = 0; x < w; ++x, ++idxP, ++idxC) {
                    _pdf[idxP] = 1.0f/w;
                    _cdf[idxC] = x/float(w);
                }
            } else {
                for (int x = 0; x < w; ++x, ++idxP, ++idxC) {
                    _pdf[idxP] /= rowWeight;
                    _cdf[idxC] /= rowWeight;
                }
            }
            _cdf[idxTail] = 1.0f;
        }

        float totalWeight = _marginalCdf.back();
        for (float &p : _marginalPdf)
            p /= totalWeight;
        for (float &c : _marginalCdf)
            c /= totalWeight;
        _marginalCdf.back() = 1.0f;
    }

    void warp(Vec2f &uv, int &row, int &column) const
    {
        row = int(std::distance(_marginalCdf.begin(), std::upper_bound(_marginalCdf.begin(), _marginalCdf.end(), uv.y())) - 1);
        uv.y() = clamp((uv.y() - _marginalCdf[row])/_marginalPdf[row], 0.0f, 1.0f);
        auto rowStart = _cdf.begin() + row*(_w + 1);
        auto rowEnd = rowStart + (_w + 1);
        column = int(std::distance(rowStart, std::upper_bound(rowStart, rowEnd, uv.x())) - 1);
        int idxC = row*(_w + 1) + column;
        int idxP = row*_w + column;
        uv.x() = clamp((uv.x() - _cdf[idxC])/_pdf[idxP], 0.0f, 1.0f);
    }

    float pdf(int row, int column) const
    {
        row    = clamp(row,    0, _h - 1);
        column = clamp(column, 0, _w - 1);
        return _pdf[row*_w + column]*_marginalPdf[row];
    }

    Vec2f unwarp(Vec2f uv, int row, int column) const
    {
        row    = clamp(row,    0, _h - 1);
        column = clamp(column, 0, _w - 1);
        int idxC = row*(_w + 1) + column;
        int idxP = row*_w + column;

        return Vec2f(
            uv.x()*_pdf[idxP] + _cdf[idxC],
            uv.y()*_marginalPdf[row] + _marginalCdf[row]
        );
    }
};

}

#endif /* DISTRIBUTION1D_HPP_ */
