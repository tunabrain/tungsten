#ifndef INTERSECTIONINFO_HPP_
#define INTERSECTIONINFO_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class Primitive;
class Bsdf;

struct IntersectionInfo
{
    Vec3f Ng;
    Vec3f Ns;
    Vec3f p;
    Vec3f w;
    Vec2f uv;
    float epsilon;

    const Primitive *primitive;
    const Bsdf *bsdf;
};

}

#endif /* INTERSECTIONINFO_HPP_ */
