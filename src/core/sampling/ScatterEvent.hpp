#ifndef SCATTEREVENT_HPP_
#define SCATTEREVENT_HPP_

#include "math/Vec.hpp"

namespace Tungsten
{

struct ScatterEvent
{
    Vec3f Ng;
    Vec3f wi, wo;
    Vec3f throughput;
    Vec3f xi;
    float pdf;
    bool switchHemisphere;
    uint16 space;

    ScatterEvent()
    : pdf(1.0f), switchHemisphere(false), space(0)
    {
    }

    bool isConsistent() const
    {
        return (wi.dot(Ng) < 0.0f) == switchHemisphere;
    }
};

}

#endif /* SCATTEREVENT_HPP_ */
