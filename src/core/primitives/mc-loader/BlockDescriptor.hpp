#ifndef BLOCKDESCRIPTOR_HPP_
#define BLOCKDESCRIPTOR_HPP_

#include "ModelResolver.hpp"
#include "BlockVariant.hpp"

#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class BlockDescriptor
{
    std::string _name;

    std::vector<BlockVariant> _variants;

public:
    BlockDescriptor(const std::string &name, JsonPtr value, ModelResolver &resolver)
    : _name(name)
    {
        if (auto variants = value["variants"])
            for (auto i : variants)
                if (i.second.isObject() || i.second.isArray())
                    _variants.emplace_back(i.first, i.second, resolver);
    }

    const std::string &name() const
    {
        return _name;
    }

    const std::vector<BlockVariant> &variants() const
    {
        return _variants;
    }

    std::vector<BlockVariant> &variants()
    {
        return _variants;
    }
};

}
}

#endif /* BLOCKDESCRIPTOR_HPP_ */
