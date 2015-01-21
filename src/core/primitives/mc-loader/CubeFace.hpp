#ifndef CUBEFACE_HPP_
#define CUBEFACE_HPP_

#include "math/MathUtil.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <array>

namespace Tungsten {

class CubeFace
{
    Vec4i _uv;
    std::string _texture;
    std::string _cullFace;
    int _rotation;
    int _tint;

public:
    CubeFace()
    : _uv(0, 0, 16, 16),
      _rotation(0),
      _tint(0)
    {
    }

    CubeFace(const rapidjson::Value &v)
    : _uv(0, 0, 16, 16),
      _rotation(0),
      _tint(0)
    {
        JsonUtils::fromJson(v, "uv", _uv);
        JsonUtils::fromJson(v, "texture", _texture);
        JsonUtils::fromJson(v, "cullface", _cullFace);
        JsonUtils::fromJson(v, "rotation", _rotation);
        JsonUtils::fromJson(v, "tintindex", _tint);
    }

    const std::string &texture() const
    {
        return _texture;
    }

    std::array<Vec2f, 4> generateUVs() const
    {
        std::array<Vec2f, 4> result{
            Vec2f(_uv[0]/16.0f, _uv[1]/16.0f),
            Vec2f(_uv[2]/16.0f, _uv[1]/16.0f),
            Vec2f(_uv[2]/16.0f, _uv[3]/16.0f),
            Vec2f(_uv[0]/16.0f, _uv[3]/16.0f)
        };

        int rotIndex = max(_rotation/90, 0) % 4;
        for (int i = 0; i < rotIndex; ++i) {
            Vec2f tmp = result[3];
            result[3] = result[2];
            result[2] = result[1];
            result[1] = result[0];
            result[0] = tmp;
        }

        return result;
    }
};

}

#endif /* CUBEFACE_HPP_ */
