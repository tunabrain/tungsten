#ifndef RECONSTRUCTIONFILTER_HPP_
#define RECONSTRUCTIONFILTER_HPP_

#include "sampling/Distribution2D.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Box.hpp"
#include "math/Vec.hpp"

#include "StringableEnum.hpp"

#include <memory>

namespace Tungsten {

#define RFILTER_RESOLUTION 31

class ReconstructionFilter
{
    enum TypeEnum
    {
        Dirac,
        Box,
        Tent,
        Gaussian,
        MitchellNetravali,
        CatmullRom,
        Lanczos,
    };

    typedef StringableEnum<TypeEnum> Type;
    friend Type;

    static float filterWidth(TypeEnum type);

    Type _type;

    float _width;
    float _binSize;
    float _invBinSize;
    float _filter[RFILTER_RESOLUTION + 1];
    float _cdf[RFILTER_RESOLUTION + 1];

    inline float mitchellNetravali(float x) const
    {
        CONSTEXPR float B = 1.0f/3.0f;
        CONSTEXPR float C = 1.0f/3.0f;
        if (x < 1.0f)
            return 1.0f/6.0f*(
                (12.0f - 9.0f*B - 6.0f*C)*x*x*x
              + (-18.0f + 12.0f*B + 6.0f*C)*x*x
              + (6.0f - 2.0f*B));
        else if (x < 2.0f)
            return 1.0f/6.0f*(
                (-B - 6.0f*C)*x*x*x
              + (6.0f*B + 30.0f*C)*x*x
              + (-12.0f*B - 48.0f*C)*x
              + (8.0f*B + 24.0f*C));
        else
            return 0.0f;
    }

    inline float catmullRom(float x) const
    {
        if (x < 1.0f)
            return 1.0f/6.0f*((12.0f - 3.0f)*x*x*x + (-18.0f + 3.0f)*x*x + 6.0f);
        else if (x < 2.0f)
            return 1.0f/6.0f*(-3.0f*x*x*x + 15.0f*x*x - 24.0f*x + 12.0f);
        else
            return 0.0f;
    }

    inline float lanczos(float x) const
    {
        if (x == 0.0f)
            return 1.0f;
        else if (x < 2.0f)
            return std::sin(PI*x)*std::sin(PI*x/2.0f)/(PI*PI*x*x/2.0f);
        else
            return 0.0f;
    }

    void precompute();

    void sample(float xi, float &u, float &pdf) const
    {
        bool negative = xi < 0.5f;
        xi = negative ? xi*2.0f : (xi - 0.5f)*2.0f;

        int idx = RFILTER_RESOLUTION - 1;
        for (int i = 0; i < RFILTER_RESOLUTION - 1; ++i) {
            if (xi < _cdf[i]) {
                idx = i;
                break;
            }
        }
        pdf = _cdf[idx] - _cdf[idx - 1];
        u = _binSize*(idx + (xi - _cdf[idx - 1])/pdf);
        pdf *= 0.5f*_invBinSize;
        if (negative)
            u = -u;
    }

    float invert(float u) const
    {
        bool negative = u < 0.0f;
        u = std::abs(u)*_invBinSize;

        int idx = min(int(u), RFILTER_RESOLUTION);

        float xi = _cdf[idx - 1] + (u - idx)*(_cdf[idx] - _cdf[idx - 1]);
        xi = negative ? xi*0.5f : 0.5f + xi*0.5f;

        return xi;
    }

    bool invert(int minX, int maxX, float x, float mu, int &pixel, float &xi) const
    {
        CONSTEXPR int MaxWidth = 2;
        CONSTEXPR int NumBins = (MaxWidth + 1)*2;
        // Simple trick to avoid round-to-zero shenanigans - we know the position on the image plane
        // can't be more than the filter width below zero, so we shift-round-shift to make sure we
        // round down always
        int ix = int(x + 2*MaxWidth) - 2*MaxWidth;
        float cx = ix + 0.5f;

        float pixelCdf[NumBins];
        for (int dx = -MaxWidth; dx <= MaxWidth; ++dx)
            pixelCdf[dx + MaxWidth] = evalApproximate(x - (cx + dx))*(ix + dx >= minX && ix + dx < maxX ? 1.0f : 0.0f);
        for (int i = 1; i < NumBins; ++i)
            pixelCdf[i] += pixelCdf[i - 1];

        if (pixelCdf[NumBins - 1] == 0.0f)
            return false;

        float target = pixelCdf[NumBins - 1]*mu;
        int idx;
        for (idx = 0; idx < NumBins - 1; ++idx)
            if (target < pixelCdf[idx])
                break;

        pixel = ix + (idx - MaxWidth);
        xi = invert(x - (pixel + 0.5f));
        return true;
    }

public:
    ReconstructionFilter(const std::string &name = "tent") : _type(name ) { precompute(); }
    ReconstructionFilter(JsonPtr value)                    : _type(value) { precompute(); }

    inline Vec2f sample(Vec2f uv, float &pdf) const
    {
        if (_type == Dirac) {
            pdf = 1.0f;
            return Vec2f(0.0f);
        } else if (_type == Box) {
            pdf = 1.0f;
            return uv - 0.5f;
        }

        Vec2f result;
        float pdfX, pdfY;
        sample(uv.x(), result.x(), pdfX);
        sample(uv.y(), result.y(), pdfY);

        pdf = pdfX*pdfY;
        return result;
    }

    inline bool invert(const Box2i &bounds, Vec2f pixel, Vec2f mu, Vec2i &pixelI, Vec2f &xi) const
    {
        if (_type == Dirac) {
            return false;
        } else if (_type == Box) {
            pixelI = Vec2i(pixel);
            xi = pixel - Vec2f(pixelI);
            return true;
        }

        bool successX = invert(bounds.min().x(), bounds.max().x(), pixel.x(), mu.x(), pixelI.x(), xi.x());
        bool successY = invert(bounds.min().y(), bounds.max().y(), pixel.y(), mu.y(), pixelI.y(), xi.y());

        return successX && successY;
    }

    float eval(float x) const
    {
        switch (_type) {
        case Dirac:
            return 0.0f;
        case Box:
            return (x >= -0.5f && x <= 0.5f) ? 1.0f : 0.0f;
        case Tent:
            return 1.0f - std::abs(x);
        case Gaussian: {
            const float Alpha = 2.0f;
            return max(std::exp(-Alpha*x*x) - std::exp(-Alpha*4.0f), 0.0f);
        } case MitchellNetravali:
            return mitchellNetravali(std::abs(x));
        case CatmullRom:
            return catmullRom(std::abs(x));
        case Lanczos:
            return lanczos(std::abs(x));
        default:
            return 0.0f;
        }
    }

    inline float evalApproximate(float x) const
    {
        return _filter[min(int(std::abs(x*_invBinSize)), RFILTER_RESOLUTION)];
    }

    float width() const
    {
        return _width;
    }

    const char *name() const
    {
        return _type.toString();
    }

    bool isDirac() const
    {
        return _type == Dirac;
    }

    bool isBox() const
    {
        return _type == Box;
    }
};

}

#endif /* RECONSTRUCTIONFILTER_HPP_ */
