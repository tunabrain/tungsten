#ifndef LENSSAMPLE_HPP_
#define LENSSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

struct LensSample
{
    Vec2u pixel;
    Vec3f d;
    float dist;
    float pdf;
    Vec3f weight;
};

}

#endif /* LENSSAMPLE_HPP_ */
