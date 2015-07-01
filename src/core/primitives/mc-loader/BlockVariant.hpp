#ifndef BLOCKVARIANT_HPP_
#define BLOCKVARIANT_HPP_

#include "ModelRef.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class BlockVariant
{
    std::string _variant;
    std::vector<ModelRef> _models;

public:
    BlockVariant(const std::string &variant, const rapidjson::Value &v, ModelResolver &resolver)
    : _variant(variant)
    {
        if (v.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v.Size(); ++i)
                if (v[i].IsObject())
                    _models.emplace_back(v[i], resolver);
        } else if (v.IsObject())
            _models.emplace_back(v, resolver);

        float weightSum = 0.0f;
        for (const ModelRef &m : _models)
            weightSum += m.weight();
        if (weightSum != 0.0f) {
            float cdf = 0.0f;
            for (ModelRef &m : _models) {
                cdf += m.weight();
                m.setWeight(cdf/weightSum);
            }
        }
    }

    std::string &variant()
    {
        return _variant;
    }

    const std::string &variant() const
    {
        return _variant;
    }

    std::vector<ModelRef> &models()
    {
        return _models;
    }

    const std::vector<ModelRef> &models() const
    {
        return _models;
    }
};

}
}

#endif /* BLOCKVARIANT_HPP_ */
