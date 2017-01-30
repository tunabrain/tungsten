#ifndef JSONUTILS_HPP_
#define JSONUTILS_HPP_

#include "JsonSerializable.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include <rapidjson/document.h>
#include <string>

namespace Tungsten {

class Path;

namespace JsonUtils {

rapidjson::Value toJson(rapidjson::Value v, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(const JsonSerializable &o, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(const std::string &value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(const char *value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(const Path &value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(bool value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(uint32 value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(int32 value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(uint64 value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(float value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(double value, rapidjson::Document::AllocatorType &allocator);
rapidjson::Value toJson(const Mat4f &value, rapidjson::Document::AllocatorType &allocator);

template<typename ElementType, unsigned Size>
rapidjson::Value toJson(const Vec<ElementType, Size> &value, rapidjson::Document::AllocatorType &allocator)
{
    if (value == value[0]) {
        return toJson(double(value[0]), allocator);
    } else {
        rapidjson::Value a(rapidjson::kArrayType);
        for (unsigned i = 0; i < Size; ++i)
            a.PushBack(value[i], allocator);

        return std::move(a);
    }
}

void addObjectMember(rapidjson::Value &v, const char *name, const JsonSerializable &o,
        rapidjson::Document::AllocatorType &allocator);

std::string jsonToString(const rapidjson::Document &document);

}

}

#endif /* JSONUTILS_HPP_ */
