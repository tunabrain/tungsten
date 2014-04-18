#ifndef JSONUTILS_HPP_
#define JSONUTILS_HPP_

#include <rapidjson/document.h>
#include <string>

#include "JsonSerializable.hpp"

#include "Debug.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

namespace Tungsten
{

class JsonUtils
{
public:
    template<typename T>
    static inline T fromJson(const rapidjson::Value &v);

    template<typename ElementType, unsigned Size>
    static Vec<ElementType, Size> fromJson(const rapidjson::Value &v)
    {
        SOFT_ASSERT(v.IsArray(), "Cannot convert Json value to vector: Value is not an array");
        SOFT_ASSERT(v.Size() == 1 || v.Size() == Size,
            "Cannot convert Json Array to vector: Invalid size");

        if (v.Size() == 1) {
            return Vec<ElementType, Size>(fromJson<ElementType>(v[0u]));
        } else {
            Vec<ElementType, Size> result;
            for (unsigned i = 0; i < Size; ++i)
                result[i] = fromJson<ElementType>(v[i]);
            return result;
        }
    }

    template<typename ElementType, unsigned Size>
    static Vec<ElementType, Size> fromJson(const rapidjson::Value &v, const Vec<ElementType, Size> &defaultValue)
    {
        if (v.IsNull())
            return defaultValue;
        else
            return fromJson<ElementType, Size>(v);
    }

    template<typename Type>
    static Type fromJson(const rapidjson::Value &v, const Type &defaultValue)
    {
        if (v.IsNull())
            return defaultValue;
        else
            return fromJson<Type>(v);
    }

    static inline const rapidjson::Value &fetchMandatoryMember(const rapidjson::Value &v, const char *name)
    {
        const rapidjson::Value::Member *member = v.FindMember(name);
        SOFT_ASSERT(member, "Json value is missing mandatory member '%s'", name);

        return member->value;
    }

    template<typename Type>
    static inline Type fromJsonMember(const rapidjson::Value &v, const char *name)
    {
        return fromJson<Type>(fetchMandatoryMember(v, name));
    }

    template<typename ElementType, unsigned Size>
    static Vec<ElementType, Size> fromJsonMember(const rapidjson::Value &v, const char *name)
    {
        return fromJson<ElementType, Size>(fetchMandatoryMember(v, name));
    }

    template<typename ElementType, unsigned Size>
    static rapidjson::Value toJsonValue(const Vec<ElementType, Size> &value, rapidjson::Document::AllocatorType &allocator)
    {
        rapidjson::Value a(rapidjson::kArrayType);
        for (unsigned i = 0; i < Size; ++i)
            a.PushBack(value[i], allocator);

        return std::move(a);
    }

    static inline rapidjson::Value toJsonValue(const Mat4f &value, rapidjson::Document::AllocatorType &allocator)
    {
        rapidjson::Value a(rapidjson::kArrayType);
        for (unsigned i = 0; i < 16; ++i)
            a.PushBack(value[i], allocator);

        return std::move(a);
    }

    static inline void addObjectMember(rapidjson::Value &v, const char *name, JsonSerializable &o, rapidjson::Document::AllocatorType &allocator)
    {
        if (o.unnamed())
            v.AddMember(name, o.toJson(allocator), allocator);
        else
            v.AddMember(name, o.name().c_str(), allocator);
    }
};

template<> bool        JsonUtils::fromJson<bool>       (const rapidjson::Value &v);
template<> float       JsonUtils::fromJson<float>      (const rapidjson::Value &v);
template<> double      JsonUtils::fromJson<double>     (const rapidjson::Value &v);
template<> uint32      JsonUtils::fromJson<uint32>     (const rapidjson::Value &v);
template<> int32       JsonUtils::fromJson<int32>      (const rapidjson::Value &v);
template<> uint64      JsonUtils::fromJson<uint64>     (const rapidjson::Value &v);
template<> int64       JsonUtils::fromJson<int64>      (const rapidjson::Value &v);
template<> std::string JsonUtils::fromJson<std::string>(const rapidjson::Value &v);
template<> Mat4f       JsonUtils::fromJson<Mat4f>      (const rapidjson::Value &v);

}

#endif /* JSONUTILS_HPP_ */
