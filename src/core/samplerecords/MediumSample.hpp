#ifndef MEDIUMSAMPLE_HPP_
#define MEDIUMSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class PhaseFunction;

struct MediumSample
{
    PhaseFunction *phase;
    Vec3f p;
    float t;
    Vec3f weight;
    float pdf;
    bool exited;
};

}

#endif /* MEDIUMSAMPLE_HPP_ */
