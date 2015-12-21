#include "Texture.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

bool Texture::scalarOrVecFromJson(const rapidjson::Value &v, const char *field, Vec3f &dst)
{
    float scalar;
    if (JsonUtils::fromJson(v, field, scalar)) {
        dst = Vec3f(scalar);
        return true;
    } else {
        return JsonUtils::fromJson(v, field, dst);
    }
}

rapidjson::Value Texture::scalarOrVecToJson(const Vec3f &src, Allocator &allocator)
{
    if (src.x() == src.y() && src.y() == src.z())
        return std::move(JsonUtils::toJson(src.x(), allocator));
    else
        return std::move(JsonUtils::toJson(src, allocator));
}

}
