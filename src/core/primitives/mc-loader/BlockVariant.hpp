#ifndef BLOCKVARIANT_HPP_
#define BLOCKVARIANT_HPP_

#include "ModelRef.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <vector>

namespace Tungsten {

class BlockVariant
{
    std::string _variant;
    std::vector<ModelRef> _models;

public:
    BlockVariant(const std::string &variant, const rapidjson::Value &v, ModelResolver &resolver)
    : _variant(variant)
    {
        if (v.IsArray()) {
            for (size_t i = 0; i < v.Size(); ++i)
                if (v[i].IsObject())
                    _models.emplace_back(v[i], resolver);
        } else if (v.IsObject())
            _models.emplace_back(v, resolver);
    }

    const std::string &variant() const
    {
        return _variant;
    }

    const std::vector<ModelRef> &models() const
    {
        return _models;
    }
};


}

#endif /* BLOCKVARIANT_HPP_ */
