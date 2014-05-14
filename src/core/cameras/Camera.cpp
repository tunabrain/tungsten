#include "Camera.hpp"

#include "io/Scene.hpp"

namespace Tungsten
{

void Camera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonUtils::fromJson(v, "file", _outputFile);
    JsonUtils::fromJson(v, "tonemap", _tonemapString);
    JsonUtils::fromJson(v, "position", _pos);
    JsonUtils::fromJson(v, "lookAt", _lookAt);
    JsonUtils::fromJson(v, "up", _up);
    JsonUtils::fromJson(v, "resolution", _res);
    JsonUtils::fromJson(v, "spp", _spp);
    if (const rapidjson::Value::Member *medium = v.FindMember("medium"))
        _medium = scene.fetchMedium(medium->value);

    precompute();
}

rapidjson::Value Camera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("file", _outputFile.c_str(), allocator);
    v.AddMember("tonemap", _tonemapString.c_str(), allocator);
    v.AddMember("position", JsonUtils::toJsonValue<float, 3>(_pos,    allocator), allocator);
    v.AddMember("lookAt",   JsonUtils::toJsonValue<float, 3>(_lookAt, allocator), allocator);
    v.AddMember("up",       JsonUtils::toJsonValue<float, 3>(_up,     allocator), allocator);
    v.AddMember("resolution", JsonUtils::toJsonValue<uint32, 2>(_res, allocator), allocator);
    v.AddMember("spp", _spp, allocator);
    if (_medium)
        JsonUtils::addObjectMember(v, "medium", *_medium,  allocator);
    return std::move(v);
}

}
