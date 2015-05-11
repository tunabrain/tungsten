#ifndef ATOMICFRAMEBUFFER_HPP_
#define ATOMICFRAMEBUFFER_HPP_

#include "math/Vec.hpp"

#include "Debug.hpp"

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

    std::unique_ptr<Vec3fa[]> _buffer;

    void atomicAdd(std::atomic<float> &dst, float add){
         float current = dst.load();
         float desired = current + add;
         while (!dst.compare_exchange_weak(current, desired))
              desired = current + add;
    }

public:
    AtomicFramebuffer(uint32 w, uint32 h)
    : _w(w),
      _h(h),
      _buffer(new Vec3fa[w*h])
    {
    }

    inline void splat(Vec2u pixel, Vec3f w)
    {
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
