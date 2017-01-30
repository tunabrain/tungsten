#include "Texture.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

rapidjson::Value Texture::scalarOrVecToJson(const Vec3f &src, Allocator &allocator)
{
    if (src.x() == src.y() && src.y() == src.z())
        return std::move(JsonUtils::toJson(src.x(), allocator));
    else
        return std::move(JsonUtils::toJson(src, allocator));
}

}
