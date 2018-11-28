#ifndef ATOMICFRAMEBUFFER_HPP_
#define ATOMICFRAMEBUFFER_HPP_

#include "ReconstructionFilter.hpp"

#include "math/Vec.hpp"

#include <memory>
#include <atomic>

namespace Tungsten {

class AtomicFramebuffer
{
    typedef Vec<std::atomic<float>, 3> Vec3fa;

    static_assert(sizeof(std::atomic<float>) == 4, "std::atomic<float> is not a simple type! "
            "This will break a lot of things");

    uint32 _w;
    uint32 _h;
    ReconstructionFilter _filter;

    std::unique_ptr<Vec3fa[]> _buffer;

    void atomicAdd(std::atomic<float> &dst, float add){
         float current = dst.load();
         float desired = current + add;
         while (!dst.compare_exchange_weak(current, desired))
              desired = current + add;
    }

public:
    AtomicFramebuffer(uint32 w, uint32 h, const ReconstructionFilter &filter)
    : _w(w),
      _h(h),
      _filter(filter),
      _buffer(new Vec3fa[w*h])
    {
        unsafeReset();
    }
    AtomicFramebuffer(AtomicFramebuffer &&o)
    : _w(o._w),
      _h(o._h),
      _filter(o._filter),
      _buffer(std::move(o._buffer))
    {
    }

    inline void splatFiltered(Vec2f pixel, Vec3f w)
    {
        if (_filter.isDirac()) {
            return;
        } else if (_filter.isBox()) {
            splat(Vec2u(pixel), w);
        } else {
            float px = pixel.x() - 0.5f;
            float py = pixel.y() - 0.5f;

            uint32 minX = max(int(px + 1.0f - _filter.width()), 0);
            uint32 maxX = min(int(px        + _filter.width()), int(_w) - 1);
            uint32 minY = max(int(py + 1.0f - _filter.width()), 0);
            uint32 maxY = min(int(py        + _filter.width()), int(_h) - 1);

            // Maximum filter width is 2 pixels
            float weightX[4], weightY[4];
            for (uint32 x = minX; x <= maxX; ++x)
                weightX[x - minX] = _filter.evalApproximate(x - px);
            for (uint32 y = minY; y <= maxY; ++y)
                weightY[y - minY] = _filter.evalApproximate(y - py);

            for (uint32 y = minY; y <= maxY; ++y)
                for (uint32 x = minX; x <= maxX; ++x)
                    splat(Vec2u(x, y), w*weightX[x - minX]*weightY[y - minY]);
        }
    }

    inline void splat(Vec2u pixel, Vec3f w)
    {
        if (std::isnan(w) || std::isinf(w))
            return;

        uint32 idx = pixel.x() + pixel.y()*_w;
        atomicAdd(_buffer[idx].x(), w.x());
        atomicAdd(_buffer[idx].y(), w.y());
        atomicAdd(_buffer[idx].z(), w.z());
    }

    inline Vec3f get(int x, int y) const
    {
        return Vec3f(
            _buffer[x + y*_w].x(),
            _buffer[x + y*_w].y(),
            _buffer[x + y*_w].z()
        );
    }

    void unsafeReset()
    {
        std::memset(&_buffer[0].x(), 0, _w*_h*sizeof(Vec3fa));
    }
};

}

#endif /* ATOMICFRAMEBUFFER_HPP_ */
