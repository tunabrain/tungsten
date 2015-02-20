#ifndef MODELREF_HPP_
#define MODELREF_HPP_

#include "ModelResolver.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class ModelRef
{
    std::string _modelPath;
    int _xRot;
    int _yRot;
    int _zRot;
    bool _uvLock;
    float _weight;

    const std::vector<TexturedQuad> *_builtModel;

public:
    ModelRef(const rapidjson::Value &v, ModelResolver &resolver)
    : _xRot(0),
      _yRot(0),
      _zRot(0),
      _uvLock(false),
      _weight(1.0f)
    {
        JsonUtils::fromJson(v, "model", _modelPath);
        JsonUtils::fromJson(v, "x", _xRot);
        JsonUtils::fromJson(v, "y", _yRot);
        JsonUtils::fromJson(v, "z", _zRot);
        JsonUtils::fromJson(v, "uvlock", _uvLock);
        JsonUtils::fromJson(v, "weight", _weight);

        _builtModel = resolver.resolveModel("block/" + _modelPath);
    }

    const std::string &modelPath() const
    {
        return _modelPath;
    }

    int xRot() const
    {
        return _xRot;
    }

    int yRot() const
    {
        return _yRot;
    }

    int zRot() const
    {
        return _zRot;
    }

    bool uvLock() const
    {
        return _uvLock;
    }

    float weight() const
    {
        return _weight;
    }

    void setWeight(float w)
    {
        _weight = w;
    }

    const std::vector<TexturedQuad> *builtModel() const
    {
        return _builtModel;
    }

    void setBuiltModel(const std::vector<TexturedQuad> *model)
    {
        _builtModel = model;
    }
};

}
}

#endif /* MODELREF_HPP_ */
