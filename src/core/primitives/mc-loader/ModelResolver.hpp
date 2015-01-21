#ifndef MODELRESOLVER_HPP_
#define MODELRESOLVER_HPP_

#include "TexturedQuad.hpp"
#include "Model.hpp"

#include <unordered_map>
#include <string>
#include <vector>

namespace Tungsten {

class ModelResolver
{
    std::unordered_map<std::string, std::string> _textureVariables;
    std::unordered_map<std::string, std::vector<TexturedQuad>> _builtModels;
    std::unordered_map<std::string, const Model *> _models;

    std::string resolveTexture(std::string var)
    {
        std::string result;
        do {
            result = _textureVariables[var];
            var = result;
        } while (!result.empty() && result[0] == '#');
        return result;
    }

public:
    ModelResolver(const std::vector<Model> &models)
    {
        for (const Model &m : models)
            _models.insert(std::make_pair(m.name(), &m));
    }

    void insertTexture(const std::string &var, const std::string &value)
    {
        _textureVariables["#" + var] = value;
    }

    void visitParent(const std::string &name, std::vector<TexturedQuad> &dst)
    {
        if (_models.count(name))
            _models[name]->instantiateQuads(dst, *this);
        else
            std::cout << "Unable to find parent " << name << std::endl;
    }

    const std::vector<TexturedQuad> *resolveModel(const std::string &name)
    {
        if (!_builtModels.count(name)) {
            if (!_models.count(name))
                return nullptr;

            _textureVariables.clear();

            std::vector<TexturedQuad> quads;
            _models[name]->instantiateQuads(quads, *this);

            for (TexturedQuad &q : quads)
                q.texture = resolveTexture(q.texture);

            _builtModels.emplace(std::make_pair(name, std::move(quads)));
        }

        return &_builtModels[name];
    }
};

}

#endif /* MODELRESOLVER_HPP_ */
