#ifndef LENSSAMPLE_HPP_
#define LENSSAMPLE_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

struct LensSample
{
    Vec2f pixel;
    Vec3f d;
    float dist;
    Vec3f weight;
};

}

#endif /* LENSSAMPLE_HPP_ */
