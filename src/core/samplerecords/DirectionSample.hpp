#ifndef DIRECTIONSAMPLE_HPP_
#define DIRECTIONSAMPLE_HPP_

namespace Tungsten {

struct DirectionSample
{
    Vec3f d;
    Vec3f weight;
    float pdf;

    DirectionSample() = default;
    DirectionSample(const Vec3f &d_)
    : d(d_)
    {
    }
};

}

#endif /* DIRECTIONSAMPLE_HPP_ */
