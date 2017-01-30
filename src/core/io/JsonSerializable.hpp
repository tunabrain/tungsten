#ifndef JSONSERIALIZABLE_HPP_
#define JSONSERIALIZABLE_HPP_

#include "JsonPtr.hpp"

#include <rapidjson/document.h>
#include <string>

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

    virtual void fromJson(JsonPtr value, const Scene &scene);
    virtual rapidjson::Value toJson(Allocator &allocator) const;

    // Loads any additional resources referenced by this object, e.g. bitmaps
    // for textures, or mesh files in the TriangleMesh. This is split from
    // fromJson to allow parsing a scene document without loading any of the
    // heavy binary data
    virtual void loadResources() {}
    // Saves any resources that can be modified during runtime (i.e. in the editor)
    // This mostly affects triangle meshes.
    // Whether this object is dirty and needs saving is tracked externally and does
    // not need to be handled here.
    virtual void saveResources() {}

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

