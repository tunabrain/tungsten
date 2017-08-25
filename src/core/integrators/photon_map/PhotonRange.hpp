#ifndef PHOTONRANGE_HPP_
#define PHOTONRANGE_HPP_

#include "Photon.hpp"

#include "math/MathUtil.hpp"

#include <vector>

namespace Tungsten {

template<typename PhotonType>
class PhotonRange
{
    PhotonType *_dst;
    uint32 _start;
    uint32 _next;
    uint32 _end;

public:
    PhotonRange()
    : _dst(nullptr), _start(0), _next(0), _end(0)
    {
    }

    PhotonRange(PhotonType *dst, uint32 start, uint32 end)
    : _dst(dst),
      _start(start),
      _next(start),
      _end(end)
    {
    }

    PhotonType &addPhoton()
    {
        return _dst[_next++];
    }

    bool full() const
    {
        return _dst == nullptr || _next == _end;
    }

    uint32 start() const
    {
        return _start;
    }

    uint32 end() const
    {
        return _end;
    }

    uint32 next() const
    {
        return _next;
    }

    void reset()
    {
        _next = _start;
    }

    void bumpNext(int offset)
    {
        _next += offset;
    }

    PhotonType *nextPtr()
    {
        return _dst + _next;
    }
};

template<typename PhotonType>
uint32 streamCompact(std::vector<PhotonRange<PhotonType>> &ranges)
{
    uint32 tail = 0;
    for (size_t i = 0; i < ranges.size(); ++i) {
        uint32 gap = ranges[i].end() - ranges[i].next();

        for (size_t t = i + 1; t < ranges.size() && gap > 0; ++t) {
            uint32 copyCount = min(gap, ranges[t].next() - ranges[t].start());
            if (copyCount > 0) {
                std::memcpy(
                    ranges[i].nextPtr(),
                    ranges[t].nextPtr() - copyCount,
                    copyCount*sizeof(PhotonType)
                );
            }
            ranges[i].bumpNext( int(copyCount));
            ranges[t].bumpNext(-int(copyCount));
            gap -= copyCount;
        }

        tail = ranges[i].next();
        if (gap > 0)
            break;
    }

    return tail;
}

typedef PhotonRange<Photon> SurfacePhotonRange;
typedef PhotonRange<VolumePhoton> VolumePhotonRange;
typedef PhotonRange<PathPhoton> PathPhotonRange;

}

#endif /* PHOTONRANGE_HPP_ */
