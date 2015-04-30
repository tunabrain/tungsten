#ifndef PHOTONRANGE_HPP_
#define PHOTONRANGE_HPP_

#include "Photon.hpp"

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

typedef PhotonRange<Photon> SurfacePhotonRange;
typedef PhotonRange<VolumePhoton> VolumePhotonRange;

}

#endif /* PHOTONRANGE_HPP_ */
