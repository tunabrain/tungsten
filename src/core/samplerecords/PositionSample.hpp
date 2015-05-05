#ifndef POSITIONSAMPLE_HPP_
#define POSITIONSAMPLE_HPP_

namespace Tungsten {

struct PositionSample
{
    Vec3f p;
    Vec3f weight;
    float pdf;

    Vec2f uv;
    Vec3f nG;
};

}

#endif /* POSITIONSAMPLE_HPP_ */
