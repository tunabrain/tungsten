#include "JsonUtils.hpp"

namespace Tungsten {

template<>
bool JsonUtils::fromJson<bool>(const rapidjson::Value &v, bool &dst)
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

template<>
bool JsonUtils::fromJson<float>(const rapidjson::Value &v, float &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<double>(const rapidjson::Value &v, double &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<uint32>(const rapidjson::Value &v, uint32 &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<int32>(const rapidjson::Value &v, int32 &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<uint64>(const rapidjson::Value &v, uint64 &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<int64>(const rapidjson::Value &v, int64 &dst)
{
    return getJsonNumber(v, dst);
}

template<>
bool JsonUtils::fromJson<std::string>(const rapidjson::Value &v, std::string &dst)
{
    if (v.IsString()) {
        dst = std::move(std::string(v.GetString()));
        return true;
    }
    return false;
}

template<>
bool JsonUtils::fromJson<Mat4f>(const rapidjson::Value &v, Mat4f &dst)
{
    if (!v.IsArray())
        return false;
    SOFT_ASSERT(v.Size() == 16, "Cannot convert Json Array to 4x4 Matrix: Invalid size");

    for (unsigned i = 0; i < 16; ++i)
        dst[i] = as<float>(v[i]);
    return true;
}


}
