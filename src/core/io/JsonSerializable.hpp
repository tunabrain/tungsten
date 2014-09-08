#ifndef JSONSERIALIZABLE_HPP_
#define JSONSERIALIZABLE_HPP_

#include <rapidjson/document.h>
#include <string>

#include "Debug.hpp"

namespace Tungsten {

class Scene;

class JsonSerializable
{
    std::string _name;

protected:
    virtual ~JsonSerializable()
    {
    }

    typedef rapidjson::Document::AllocatorType Allocator;

public:
    JsonSerializable() = default;
    JsonSerializable(const std::string &name);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene);
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    void setName(const std::string &name)
    {
        _name = name;
    }

    const std::string &name() const
    {
        return _name;
    }

    bool unnamed() const
    {
        return _name.empty();
    }
};


}



#endif /* JSONSERIALIZABLE_HPP_ */

