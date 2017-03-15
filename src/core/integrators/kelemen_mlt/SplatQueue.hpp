#ifndef SPLATQUEUE_HPP_
#define SPLATQUEUE_HPP_

#include "cameras/AtomicFramebuffer.hpp"

#include "integrators/bidirectional_path_tracer/ImagePyramid.hpp"

#include "math/Vec.hpp"

#include <memory>

namespace Tungsten {

class SplatQueue
{
    struct FilteredSplat
    {
        int s, t;
        Vec2f pixel;
        Vec3f value;
    };
    struct Splat
    {
        int s, t;
        Vec2u pixel;
        Vec3f value;
    };

    int _maxSplats;
    int _filteredSplatCount;
    int _splatCount;
    float _totalLuminance;
    std::unique_ptr<FilteredSplat[]> _filteredSplats;
    std::unique_ptr<Splat[]> _splats;

public:
    SplatQueue(int maxSplats)
    : _maxSplats(maxSplats),
      _filteredSplatCount(0),
      _splatCount(0),
      _totalLuminance(0.0f),
      _filteredSplats(new FilteredSplat[_maxSplats]),
      _splats(new Splat[_maxSplats])
    {
    }

    void clear()
    {
        _filteredSplatCount = 0;
        _splatCount = 0;
        _totalLuminance = 0.0f;
    }

    void addSplat(int s, int t, Vec2u pixel, Vec3f value)
    {
        if (std::isnan(value) || std::isinf(value))
            return;
        _splats[_splatCount++] = Splat{s, t, pixel, value};
        _totalLuminance += value.luminance();
    }

    void addFilteredSplat(int s, int t, Vec2f pixel, Vec3f value)
    {
        if (std::isnan(value) || std::isinf(value) || std::isnan(pixel) || std::isinf(pixel))
            return;
        _filteredSplats[_filteredSplatCount++] = FilteredSplat{s, t, pixel, value};
        _totalLuminance += value.luminance();
    }

    float totalLuminance() const
    {
        return _totalLuminance;
    }

    void apply(AtomicFramebuffer &buffer, float scale)
    {
        for (int i = 0; i < _filteredSplatCount; ++i)
            buffer.splatFiltered(_filteredSplats[i].pixel, _filteredSplats[i].value*scale);
        for (int i = 0; i < _splatCount; ++i)
            buffer.splat(_splats[i].pixel, _splats[i].value*scale);
    }

    void apply(ImagePyramid &pyramid, float scale)
    {
        for (int i = 0; i < _filteredSplatCount; ++i)
            pyramid.splatFiltered(_filteredSplats[i].s, _filteredSplats[i].t, _filteredSplats[i].pixel, _filteredSplats[i].value*scale);
        for (int i = 0; i < _splatCount; ++i)
            pyramid.splat(_splats[i].s, _splats[i].t, _splats[i].pixel, _splats[i].value*scale);
    }
};

}

#endif /* SPLATQUEUE_HPP_ */
