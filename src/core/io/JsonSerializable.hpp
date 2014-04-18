#ifndef JSONSERIALIZABLE_HPP_
#define JSONSERIALIZABLE_HPP_

#include <rapidjson/document.h>
#include <string>

#include "Debug.hpp"

namespace Tungsten
{


class JsonSerializable
{
    std::string _name;

protected:
    virtual ~JsonSerializable()
    {
    }

    typedef rapidjson::Document::AllocatorType Allocator;

public:
    JsonSerializable()
    {
    }

    JsonSerializable(const std::string &name)
    : _name(name)
    {
    }

    JsonSerializable(const rapidjson::Value &v)
    {
        const rapidjson::Value::Member *nameValue = v.FindMember("name");
        if (nameValue && nameValue->value.IsString())
            _name = nameValue->value.GetString();
    }

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

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(rapidjson::kObjectType);
        if (!unnamed())
            v.AddMember("name", _name.c_str(), allocator);
        return std::move(v);
    }
};


}



#endif /* JSONSERIALIZABLE_HPP_ */

