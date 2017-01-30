#ifndef BLOCKVARIANT_HPP_
#define BLOCKVARIANT_HPP_

#include "ModelRef.hpp"

#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class BlockVariant
{
    std::string _variant;
    std::vector<ModelRef> _models;

public:
    BlockVariant(const std::string &variant, JsonPtr value, ModelResolver &resolver)
    : _variant(variant)
    {
        if (value.isArray()) {
            for (unsigned i = 0; i < value.size(); ++i)
                if (value[i].isObject())
                    _models.emplace_back(value[i], resolver);
        } else if (value.isObject())
            _models.emplace_back(value, resolver);

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
