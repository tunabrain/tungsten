#ifndef JSONUTILS_HPP_
#define JSONUTILS_HPP_

#include <rapidjson/document.h>
#include <string>

#include "JsonSerializable.hpp"

#include "Debug.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

namespace Tungsten {

class JsonUtils
{
public:
    static inline const rapidjson::Value &fetchMember(const rapidjson::Value &v, const char *name)
    {
        const rapidjson::Value::Member *member = v.FindMember(name);
        ASSERT(member, "Json value is missing mandatory member '%s'", name);
        return member->value;
    }

    template<typename T>
    static inline bool fromJson(const rapidjson::Value &v, T &dst);

    template<typename T>
    static inline T as(const rapidjson::Value &v)
    {
        T result;
        if (!fromJson(v, result)) {
            FAIL("Conversion from JSON datatype failed");
            return T();
        }
        return result;
    }

    template<typename T>
    static inline T as(const rapidjson::Value &v, const char *name)
    {
        return as<T>(fetchMember(v, name));
    }

    template<typename ElementType, unsigned Size>
    static bool fromJson(const rapidjson::Value &v, Vec<ElementType, Size> &dst)
    {
        if (!v.IsArray())
            return false;
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
    static inline bool fromJson(const rapidjson::Value &v, const char *field, T &dst)
    {
        const rapidjson::Value::Member *member = v.FindMember(field);
        if (!member)
            return false;

        return fromJson(member->value, dst);
    }

    static rapidjson::Value toJsonValue(float value, rapidjson::Document::AllocatorType &/*allocator*/)
    {
        return std::move(rapidjson::Value(double(value)));
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

    static inline void addObjectMember(rapidjson::Value &v, const char *name, const JsonSerializable &o, rapidjson::Document::AllocatorType &allocator)
    {
        if (o.unnamed())
            v.AddMember(name, o.toJson(allocator), allocator);
        else
            v.AddMember(name, o.name().c_str(), allocator);
    }
};

template<> bool JsonUtils::fromJson<bool>       (const rapidjson::Value &v, bool &dst);
template<> bool JsonUtils::fromJson<float>      (const rapidjson::Value &v, float &dst);
template<> bool JsonUtils::fromJson<double>     (const rapidjson::Value &v, double &dst);
template<> bool JsonUtils::fromJson<uint32>     (const rapidjson::Value &v, uint32 &dst);
template<> bool JsonUtils::fromJson<int32>      (const rapidjson::Value &v, int32 &dst);
template<> bool JsonUtils::fromJson<uint64>     (const rapidjson::Value &v, uint64 &dst);
template<> bool JsonUtils::fromJson<int64>      (const rapidjson::Value &v, int64 &dst);
template<> bool JsonUtils::fromJson<std::string>(const rapidjson::Value &v, std::string &dst);
template<> bool JsonUtils::fromJson<Mat4f>      (const rapidjson::Value &v, Mat4f &dst);

}

#endif /* JSONUTILS_HPP_ */
