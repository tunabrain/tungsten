#ifndef QUADMATERIAL_HPP_
#define QUADMATERIAL_HPP_

#include "textures/BitmapTexture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/Box.hpp"
#include "math/Vec.hpp"

#include <memory>

namespace Tungsten {
namespace MinecraftLoader {

struct QuadMaterial
{
    std::shared_ptr<Bsdf> bsdf;
    std::shared_ptr<Bsdf> emitterBsdf;

    Box2f opaqueBounds;
    Box2f emitterOpaqueBounds;

    std::shared_ptr<BitmapTexture> emission;
    float primaryScale, secondaryScale;
    float sampleWeight;
};

}
}

#endif /* QUADMATERIAL_HPP_ */
