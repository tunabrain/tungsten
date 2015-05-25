#ifndef RECONSTRUCTIONFILTER_HPP_
#define RECONSTRUCTIONFILTER_HPP_

#include "sampling/Distribution2D.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "Debug.hpp"

#include <memory>

namespace Tungsten {

#define RFILTER_RESOLUTION 31

class ReconstructionFilter
{
    enum Type {
        Dirac,
        Box,
        Tent,
        Gaussian,
        MitchellNetravali,
        CatmullRom,
        Lanczos,
    };

    static Type stringToType(const std::string &s);
    static float filterWidth(Type type);

    std::string _typeString;
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
        u = clamp(_binSize*(idx + (xi - _cdf[idx - 1])/pdf), 0.0f, 1.0f);
        pdf *= 0.5f*_invBinSize;
        if (negative)
            u = -u;
    }

public:
    ReconstructionFilter(const std::string &name = "tent")
    : _typeString(name),
      _type(stringToType(name))
    {
        precompute();
    }

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

    float eval(float x) const
    {
        switch (_type) {
        case Dirac:
            return 0.0f;
        case Box:
            return (x >= -0.5f && x <= -0.5f) ? 1.0f : 0.0f;
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

    const std::string &name() const
    {
        return _typeString;
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
