#ifndef BLOCKDESCRIPTOR_HPP_
#define BLOCKDESCRIPTOR_HPP_

#include "ModelResolver.hpp"
#include "BlockVariant.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <vector>

namespace Tungsten {

class BlockDescriptor
{
    std::string _name;

    std::vector<BlockVariant> _variants;

public:
    BlockDescriptor(const std::string &name, const rapidjson::Value &v, ModelResolver &resolver)
    : _name(name)
    {
        const rapidjson::Value::Member *variants = v.FindMember("variants");

        if (variants && variants->value.IsObject()) {
            const rapidjson::Value::Member *begin = variants->value.MemberBegin();
            const rapidjson::Value::Member *end   = variants->value.MemberEnd();

            for (auto i = begin; i < end; ++i) {
                if (i->value.IsObject() || i->value.IsArray())
                    _variants.emplace_back(i->name.GetString(), i->value, resolver);
            }
        }
    }

    const std::string &name() const
    {
        return _name;
    }

    const std::vector<BlockVariant> &variants() const
    {
        return _variants;
    }
};

}

#endif /* BLOCKDESCRIPTOR_HPP_ */
