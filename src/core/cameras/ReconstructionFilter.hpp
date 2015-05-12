#ifndef RECONSTRUCTIONFILTER_HPP_
#define RECONSTRUCTIONFILTER_HPP_

#include "sampling/Distribution2D.hpp"

#include "math/MathUtil.hpp"
#include "math/Bessel.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "Debug.hpp"

#include <memory>

namespace Tungsten {

// Note: Although this class can load and store to Json, it does not qualify
// as a JsonSerializable. It can never exist on its own, only as
// a Camera member, so it does not derive from the serializable base
// class.

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
        Airy,
    };

    static CONSTEXPR int Resolution = 256;

    static Type stringToType(const std::string &s);

    std::string _typeString;
    Type _type;

    float _width;
    float _alpha;

public:
    float _normalizationFactor;
    float _offset;
    std::unique_ptr<Distribution2D> _footprint;

    inline float mitchellNetravali1D(float x) const
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

    inline float catmullRom1D(float x) const
    {
        if (x < 1.0f)
            return 1.0f/6.0f*((12.0f - 3.0f)*x*x*x + (-18.0f + 3.0f)*x*x + 6.0f);
        else if (x < 2.0f)
            return 1.0f/6.0f*(-3.0f*x*x*x + 15.0f*x*x - 24.0f*x + 12.0f);
        else
            return 0.0f;
    }

    inline float lanczos1D(float x) const
    {
        if (x == 0.0f)
            return 1.0f;
        else if (x < _width)
            return std::sin(PI*x)*std::sin(PI*x/_width)/(PI*PI*x*x);
        else
            return 0.0f;
    }

    float defaultWidth();
    void precompute();

public:
    ReconstructionFilter()
    : _typeString("airy"),
      _type(Airy),
      _width(6.0f),
      _alpha(1.5f),
      _normalizationFactor(1.0f),
      _offset(0.0f)
    {
        precompute();
    }

    void fromJson(const rapidjson::Value &v);
    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const;

    inline Vec2f sample(Vec2f uv, float &weight, float &pdf) const
    {
        if (_type == Dirac) {
            weight = 1.0f;
            pdf = 1.0f;
            return Vec2f(0.0f);
        } else if (_type == Box) {
            weight = 1.0f;
            pdf = 1.0f;
            return uv*0.5f - 0.5f;
        }

        int row, col;
        _footprint->warp(uv, row, col);
        uv = ((uv + Vec2f(float(row), float(col)))*(2.0f/Resolution) - 1.0f)*_width;
        float area = _width*_width*(4.0f/(Resolution*Resolution));

        pdf = _footprint->pdf(row, col);
        weight = eval(uv)*area/pdf;
        return uv;
    }

    inline float eval(Vec2f uv) const
    {
        switch (_type) {
        case Dirac:
            return 0.0f;
        case Box:
            if (uv.x() >= -1.0f && uv.y() >= -1.0f && uv.x() <= 1.0f && uv.y() <= 1.0f)
                return 1.0f;
            else
                return 0.0f;
        case Tent:
            return (3.0f*INV_PI)*max(1.0f - uv.length(), 0.0f);
        case Gaussian:
            return _normalizationFactor*max(std::exp(-_alpha*uv.lengthSq()) - _offset, 0.0f);
        case MitchellNetravali:
            return
                mitchellNetravali1D(std::abs(uv.x()))*
                mitchellNetravali1D(std::abs(uv.y()));
        case CatmullRom:
            return
                catmullRom1D(std::abs(uv.x()))*
                catmullRom1D(std::abs(uv.y()));
        case Lanczos:
            return _normalizationFactor*lanczos1D(std::abs(uv.x()))*lanczos1D(std::abs(uv.y()));
        case Airy: {
            float r = uv.length()*3.5f;
            if (r == 0.0f)
                return _normalizationFactor;
            else if (r < _width)
                return _normalizationFactor*sqr(2.0f*Bessel::J1(r)/r);
            else
                return 0.0f;
        }
        default:
            return 0.0f;
        }
    }
};

}



#endif /* RECONSTRUCTIONFILTER_HPP_ */
