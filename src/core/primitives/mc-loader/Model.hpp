#ifndef MODEL_HPP_
#define MODEL_HPP_

#include "CubicElement.hpp"
#include "TexturedQuad.hpp"

#include "io/JsonUtils.hpp"

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

    void loadTextures(const rapidjson::Value &textures)
    {
        for (auto i = textures.MemberBegin(); i != textures.MemberEnd(); ++i)
            if (i->value.IsString())
                _textures.emplace_back(i->name.GetString(), i->value.GetString());
    }

    void loadElements(const rapidjson::Value &elements)
    {
        for (rapidjson::SizeType i = 0; i < elements.Size(); ++i)
            if (elements[i].IsObject())
                _elements.emplace_back(elements[i]);
    }

public:
    Model(const std::string &name, const rapidjson::Value &v)
    : _name(name),
      _ambientOcclusion(true)
    {
        JsonUtils::fromJson(v, "parent", _parent);
        JsonUtils::fromJson(v, "ambientocclusion", _ambientOcclusion);

        const rapidjson::Value::Member *textures = v.FindMember("textures");
        const rapidjson::Value::Member *elements = v.FindMember("elements");

        if (textures && textures->value.IsObject())
            loadTextures(textures->value);
        if (elements && elements->value.IsArray())
            loadElements(elements->value);
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
