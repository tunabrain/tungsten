#include "JsonUtils.hpp"
#include "Path.hpp"

namespace Tungsten {

namespace JsonUtils {

const rapidjson::Value &fetchMember(const rapidjson::Value &v, const char *name)
{
    const rapidjson::Value::Member *member = v.FindMember(name);
    ASSERT(member != nullptr, "Json value is missing mandatory member '%s'", name);
    return member->value;
}

bool fromJson(const rapidjson::Value &v, bool &dst)
{
    if (v.IsBool()) {
        dst = v.GetBool();
        return true;
    }
    return false;
}

template<typename T>
bool getJsonNumber(const rapidjson::Value &v, T &dst) {
    if (v.IsDouble())
        dst = T(v.GetDouble());
    else if (v.IsInt())
        dst = T(v.GetInt());
    else if (v.IsUint())
        dst = T(v.GetUint());
    else if (v.IsInt64())
        dst = T(v.GetInt64());
    else if (v.IsUint64())
        dst = T(v.GetUint64());
    else
        return false;
    return true;
}

bool fromJson(const rapidjson::Value &v, float &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, double &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, uint32 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, int32 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, uint64 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, int64 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, std::string &dst)
{
    if (v.IsString()) {
        dst = std::move(std::string(v.GetString()));
        return true;
    }
    return false;
}

bool fromJson(const rapidjson::Value &v, Mat4f &dst)
{
    if (!v.IsArray())
        return false;
    ASSERT(v.Size() == 16, "Cannot convert Json Array to 4x4 Matrix: Invalid size");

    for (unsigned i = 0; i < 16; ++i)
        dst[i] = as<float>(v[i]);
    return true;
}

bool fromJson(const rapidjson::Value &v, Path &dst)
{
    std::string path;
    bool result = fromJson(v, path);

    dst = Path(path);
    dst.freezeWorkingDirectory();

    return result;
}

rapidjson::Value toJsonValue(float value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return std::move(rapidjson::Value(double(value)));
}

rapidjson::Value toJsonValue(const Mat4f &value, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value a(rapidjson::kArrayType);
    for (unsigned i = 0; i < 16; ++i)
        a.PushBack(value[i], allocator);

    return std::move(a);
}

void addObjectMember(rapidjson::Value &v, const char *name, const JsonSerializable &o,
        rapidjson::Document::AllocatorType &allocator)
{
    if (o.unnamed())
        v.AddMember(name, o.toJson(allocator), allocator);
    else
        v.AddMember(name, o.name().c_str(), allocator);
}

}

}
