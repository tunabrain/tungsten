#ifndef MODEL_HPP_
#define MODEL_HPP_

#include "CubicElement.hpp"
#include "TexturedQuad.hpp"

#include <utility>
#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class ModelResolver;

class Model
{
    std::string _name;
    std::string _parent;
    bool _ambientOcclusion;

    std::vector<std::pair<std::string, std::string>> _textures;
    std::vector<CubicElement> _elements;

    void loadTextures(JsonPtr textures)
    {
        for (auto i : textures)
            if (i.second.isString())
                _textures.emplace_back(i.first, i.second.cast<std::string>());
    }

    void loadElements(JsonPtr elements)
    {
        for (unsigned i = 0; i < elements.size(); ++i)
            if (elements[i].isObject())
                _elements.emplace_back(elements[i]);
    }

public:
    Model(const std::string &name, JsonPtr value)
    : _name(name),
      _ambientOcclusion(true)
    {
        value.getField("parent", _parent);
        value.getField("ambientocclusion", _ambientOcclusion);

        if (auto textures = value["textures"])
            loadTextures(textures);
        if (auto elements = value["elements"])
            loadElements(elements);
    }

    const std::string &name() const
    {
        return _name;
    }

    void instantiateQuads(std::vector<TexturedQuad> &dst, ModelResolver &resolver) const;
};

}
}

#endif /* MODEL_HPP_ */
