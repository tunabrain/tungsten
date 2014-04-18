#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include <iostream>

#include "extern/stbi/stb_image.h"

#include "math/MathUtil.hpp"

#include "io/FileUtils.hpp"

#include "Debug.hpp"

namespace Tungsten
{

union Rgba
{
    uint32 rgba;
    uint8 c[4];

    Rgba() = default;

    Rgba(uint8 t)
    : c{t, t, t, t}
    {
    }
};

template<typename Texel, typename Value, int Channels>
class ImageBuffer
{
    std::string _srcDir;
    std::string _path;
    Texel *_texels;
    int _w;
    int _h;
    int _channels;
    bool _invalid;

    Vec4f normalize(Rgba x) const
    {
        return Vec4f(
            x.c[0]*(1.0f/256.0f),
            x.c[1]*(1.0f/256.0f),
            x.c[2]*(1.0f/256.0f),
            x.c[3]*(1.0f/256.0f)
        );
    }

    Vec4f lerp(Rgba x00, Rgba x01, Rgba x10, Rgba x11, float u, float v) const
    {
        return (normalize(x00)*(1.0f - u) + normalize(x01)*u)*(1.0f - v) +
               (normalize(x10)*(1.0f - u) + normalize(x11)*u)*v;
    }

    float lerp(uint8 x00, uint8 x01, uint8 x10, uint8 x11, float u, float v) const
    {
        return ((x00*(1.0f/256.0f))*(1.0f - u) + (x01*(1.0f/256.0f))*u)*(1.0f - v) +
               ((x10*(1.0f/256.0f))*(1.0f - u) + (x11*(1.0f/256.0f))*u)*v;
    }

public:
    explicit ImageBuffer(const std::string &path)
    : _srcDir( FileUtils::getCurrentDir()),
      _path(path),
      _invalid(true)
    {
        if (Channels == 1) {
            uint8 *tmp = stbi_load(path.c_str(), &_w, &_h, &_channels, 0);

            if (_channels == 4) {
                _texels = reinterpret_cast<Texel *>(tmp);
                for (size_t i = 0; i < _w*_h; ++i)
                    _texels[i] = Texel(tmp[i*4 + 3]);
            } else {
                stbi_image_free(tmp);
                _texels = reinterpret_cast<Texel *>(stbi_load(path.c_str(), &_w, &_h, &_channels, Channels));
            }
        } else
            _texels = reinterpret_cast<Texel *>(stbi_load(path.c_str(), &_w, &_h, &_channels, Channels));

        if (!_texels) {
            DBG("Unable to load texture at '%s' (current dir '%s')", path, FileUtils::getCurrentDir());
            _invalid = true;
            _texels = new Texel[2*2];
            _w = 2;
            _h = 2;
            _channels = Channels;
            _texels[0] = _texels[3] = Texel(255);
            _texels[1] = _texels[2] = Texel(0);
        }
    }

    ~ImageBuffer()
    {
        if (_invalid)
            delete[] _texels;
        else
            stbi_image_free(_texels);
    }

    Value operator[](const Vec2f &uv) const
    {
        float u = uv.x()*_w;
        float v = (1.0f - uv.y())*_h;
        int iu = int(u);
        int iv = int(v);
        u -= iu;
        v -= iv;
        iu = ((iu % _w) + _w) % _w;
        iv = ((iv % _h) + _h) % _h;
        iu = clamp(iu, 0, _w - 2);
        iv = clamp(iv, 0, _h - 2);

        int idx = iu + iv*_w;

        return Value(lerp(
            _texels[idx],
            _texels[idx + 1],
            _texels[idx + _w],
            _texels[idx + _w + 1],
            u,
            v
        ));
    }

    void saveData() const
    {
        if (FileUtils::getCurrentDir() != _srcDir)
            FileUtils::copyFile(FileUtils::addSlash(_srcDir) + _path, _path, true);
    }

    std::string fullPath() const
    {
        return std::move(FileUtils::addSlash(_srcDir) + _path);
    }

    const std::string &path() const
    {
        return _path;
    }

    void setPath(const std::string &s)
    {
        _path = s;
    }
};

typedef ImageBuffer<uint8, float, 1> TextureA;
typedef ImageBuffer<Rgba, Vec4f, 4> TextureRgba;

}

#endif /* TEXTURE_HPP_ */
