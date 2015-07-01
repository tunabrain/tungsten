#ifndef MODELRESOLVER_HPP_
#define MODELRESOLVER_HPP_

#include "TexturedQuad.hpp"
#include "Model.hpp"

#include <unordered_map>
#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

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

            // Deal with texture overlays created by duplicate quads
            std::unordered_map<Vec<float, 12>, int> existingQuads;
            std::vector<TexturedQuad> filteredQuads;
            filteredQuads.reserve(quads.size());

            for (size_t i = 0; i < quads.size(); ++i) {
                Vec<float, 12> key(
                    quads[i].p0.x(), quads[i].p0.y(), quads[i].p0.z(),
                    quads[i].p1.x(), quads[i].p1.y(), quads[i].p1.z(),
                    quads[i].p2.x(), quads[i].p2.y(), quads[i].p2.z(),
                    quads[i].p3.x(), quads[i].p3.y(), quads[i].p3.z()
                );
                auto iter = existingQuads.find(key);
                if (iter != existingQuads.end()) {
                    filteredQuads[iter->second].overlay = quads[i].texture;
                    filteredQuads[iter->second].tintIndex = quads[i].tintIndex;
                } else {
                    existingQuads.insert(std::make_pair(key, int(filteredQuads.size())));
                    filteredQuads.push_back(quads[i]);
                }
            }

            _builtModels.emplace(std::make_pair(name, std::move(filteredQuads)));
        }

        return &_builtModels[name];
    }

    std::unordered_map<std::string, std::vector<TexturedQuad>> &builtModels()
    {
        return _builtModels;
    }
};

}
}

#endif /* MODELRESOLVER_HPP_ */
