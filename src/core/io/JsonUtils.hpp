#ifndef JSONUTILS_HPP_
#define JSONUTILS_HPP_

#include "JsonSerializable.hpp"

#include "Debug.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include <rapidjson/document.h>
#include <string>

namespace Tungsten {

class Path;

namespace JsonUtils {

const rapidjson::Value &fetchMember(const rapidjson::Value &v, const char *name);

bool fromJson(const rapidjson::Value &v, bool &dst);
bool fromJson(const rapidjson::Value &v, float &dst);
bool fromJson(const rapidjson::Value &v, double &dst);
bool fromJson(const rapidjson::Value &v, uint32 &dst);
bool fromJson(const rapidjson::Value &v, int32 &dst);
bool fromJson(const rapidjson::Value &v, uint64 &dst);
bool fromJson(const rapidjson::Value &v, int64 &dst);
bool fromJson(const rapidjson::Value &v, std::string &dst);
bool fromJson(const rapidjson::Value &v, Mat4f &dst);
bool fromJson(const rapidjson::Value &v, Path &dst);

template<typename ElementType, unsigned Size>
bool fromJson(const rapidjson::Value &v, Vec<ElementType, Size> &dst);

template<typename T>
T as(const rapidjson::Value &v)
{
    T result;
    if (!fromJson(v, result)) {
        FAIL("Conversion from JSON datatype failed");
        return T();
    }
    return result;
}

template<typename T>
T as(const rapidjson::Value &v, const char *name)
{
    return as<T>(fetchMember(v, name));
}

template<typename ElementType, unsigned Size>
bool fromJson(const rapidjson::Value &v, Vec<ElementType, Size> &dst)
{
    if (!v.IsArray()) {
        dst = Vec<ElementType, Size>(as<ElementType>(v));
        return true;
    }
    ASSERT(v.Size() == 1 || v.Size() == Size,
        "Cannot convert Json Array to vector: Invalid size. Expected 1 or %d, received %d", Size, v.Size());

    if (v.Size() == 1)
        dst = Vec<ElementType, Size>(as<ElementType>(v[0u]));
    else
        for (unsigned i = 0; i < Size; ++i)
            dst[i] = as<ElementType>(v[i]);
    return true;
}

template<typename T>
inline bool fromJson(const rapidjson::Value &v, const char *field, T &dst)
{
    const rapidjson::Value::Member *member = v.FindMember(field);
    if (!member)
        return false;

    return fromJson(member->value, dst);
}

rapidjson::Value toJsonValue(double value, rapidjson::Document::AllocatorType &/*allocator*/);
rapidjson::Value toJsonValue(const Mat4f &value, rapidjson::Document::AllocatorType &allocator);

template<typename ElementType, unsigned Size>
rapidjson::Value toJsonValue(const Vec<ElementType, Size> &value, rapidjson::Document::AllocatorType &allocator)
{
    if (value == value[0]) {
        return toJsonValue(double(value[0]), allocator);
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
