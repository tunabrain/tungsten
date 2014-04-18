#include "JsonUtils.hpp"

namespace Tungsten
{

template<>
bool JsonUtils::fromJson<bool>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsBool(), "Cannot convert JSON value to boolean");
    return v.GetBool();
}

template<>
float JsonUtils::fromJson<float>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to float");
    return float(v.GetDouble());
}

template<>
double JsonUtils::fromJson<double>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to double");
    return v.GetDouble();
}

template<>
uint32 JsonUtils::fromJson<uint32>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to int");
    return v.GetUint();
}

template<>
int32 JsonUtils::fromJson<int32>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to int");
    return v.GetInt();
}

template<>
uint64 JsonUtils::fromJson<uint64>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to int");
    return v.GetUint64();
}

template<>
int64 JsonUtils::fromJson<int64>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsNumber(), "Cannot convert JSON value to int");
    return v.GetInt64();
}

template<>
std::string JsonUtils::fromJson<std::string>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsString(), "Cannot convert JSON value to string");
    return std::move(std::string(v.GetString()));
}

template<>
Mat4f JsonUtils::fromJson<Mat4f>(const rapidjson::Value &v)
{
    SOFT_ASSERT(v.IsArray(), "Cannot convert Json value to matrix: Value is not an array");
    SOFT_ASSERT(v.Size() == 16, "Cannot convert Json Array to vector: Invalid size");

    Mat4f result;
    for (unsigned i = 0; i < 16; ++i)
        result[i] = fromJson<float>(v[i]);
    return result;
}


}
