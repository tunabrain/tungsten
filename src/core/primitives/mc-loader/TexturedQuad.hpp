#ifndef TEXTUREDQUAD_HPP_
#define TEXTUREDQUAD_HPP_

#include "math/Vec.hpp"

#include <string>

namespace Tungsten {
namespace MinecraftLoader {

struct TexturedQuad
{
    std::string texture;
    std::string overlay;
    int tintIndex;
    Vec3f p0;
    Vec2f uv0;
    Vec3f p1;
    Vec2f uv1;
    Vec3f p2;
    Vec2f uv2;
    Vec3f p3;
    Vec2f uv3;
};

}
}

#endif /* TEXTUREDQUAD_HPP_ */
