#ifndef LIGHTSAMPLE_HPP_
#define LIGHTSAMPLE_HPP_

namespace Tungsten
{

struct LightSample
{
    Vec2f xi;
    Vec3f p;
    Vec3f n;

    Vec3f q;
    Vec3f w;
    Vec3f L;
    float r;
    float pdf;
};

}

#endif /* LIGHTSAMPLE_HPP_ */
