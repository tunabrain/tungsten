#include "Model.hpp"
#include "ModelResolver.hpp"

namespace Tungsten {
namespace MinecraftLoader {

void Model::instantiateQuads(std::vector<TexturedQuad> &dst, ModelResolver &resolver) const
{
    if (!_parent.empty())
        resolver.visitParent(_parent, dst);

    for (const CubicElement &e : _elements)
        e.instantiateQuads(dst);

    for (const auto &tex : _textures)
        resolver.insertTexture(tex.first, tex.second);
}

}
}
