#ifndef BITMAPTEXTURE_HPP_
#define BITMAPTEXTURE_HPP_

#include "Texture.hpp"

#include "extern/stbi/stb_image.h"

#include "math/MathUtil.hpp"

#include "io/FileUtils.hpp"

#include "Debug.hpp"

#include <iostream>

namespace Tungsten {

struct Rgba
{
    uint8 c[4];

    Rgba() = default;

    Rgba(uint8 t)
    : c{t, t, t, t}
    {
    }

    Rgba(uint8 r, uint8 g, uint8 b, uint8 a)
    : c{r, g, b, a}
    {
    }

    Rgba operator-(const Rgba &o) const
    {
        return Rgba(c[0] - o.c[0], c[1] - o.c[1], c[2] - o.c[2], c[3] - o.c[3]);
    }
};

inline Rgba min(const Rgba &a, const Rgba &b)
{
    Rgba result;
    result.c[0] = min(a.c[0], b.c[0]);
    result.c[1] = min(a.c[1], b.c[1]);
    result.c[2] = min(a.c[2], b.c[2]);

    return result;
}

inline Rgba max(const Rgba &a, const Rgba &b)
{
    Rgba result;
    result.c[0] = max(a.c[0], b.c[0]);
    result.c[1] = max(a.c[1], b.c[1]);
    result.c[2] = max(a.c[2], b.c[2]);

    return result;
}

template<bool Scalar, int Dimension>
class BitmapTexture : public Texture<Scalar, Dimension>
{
};

template<bool Scalar>
class BitmapTexture<Scalar, 2> : public Texture<Scalar, 2>
{
protected:
    typedef JsonSerializable::Allocator Allocator;
    typedef typename Texture<Scalar, 2>::Value Value;

    std::string _srcDir;
    std::string _path;

    Value _min, _max, _avg;
    int _w;
    int _h;

    float lerp(float x00, float x01, float x10, float x11, float u, float v) const
    {
        return (x00*(1.0f - u) + x01*u)*(1.0f - v) +
               (x10*(1.0f - u) + x11*u)*v;
    }

    Vec3f lerp(Vec3f x00, Vec3f x01, Vec3f x10, Vec3f x11, float u, float v) const
    {
        return (x00*(1.0f - u) + x01*u)*(1.0f - v) +
               (x10*(1.0f - u) + x11*u)*v;
    }

    virtual Value get(int x, int y) const = 0;

public:
    virtual ~BitmapTexture() {}

    virtual void fromJson(const rapidjson::Value &/*v*/, const Scene &/*scene*/) override final
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        return std::move(rapidjson::Value(_path.c_str(), allocator));
    }


    void derivativesBad(const Vec2f &uv, Vec<Value, 2> &derivs) const
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

        Value p00 = get(iu, iv);
        Value p10 = get(iu + 1, iv);
        Value p01 = get(iu, iv + 1);
        Value p11 =  get(iu + 1, iv + 1);
        Value tmp = p01 + p10 - p11;
        derivs.x() = (p10 + p00*(v - 1.0) - tmp*v)*float(_w);
        derivs.x() = (p01 + p00*(u - 1.0) - tmp*u)*float(_h);
    }

    void derivatives(const Vec2f &uv, Vec<Value, 2> &derivs) const override final
    {
        derivs = Vec<Value, 2>(Value(0.0f));
        float u = uv.x()*_w;
        float v = (1.0f - uv.y())*_h;
        int iu = int(u);
        int iv = int(v);
        u -= iu;
        v -= iv;
        iu = ((iu % _w) + _w) % _w;
        iv = ((iv % _h) + _h) % _h;
        int x0 = iu - 1, x1 = iu, x2 = (iu + 1) % _w, x3 = (iu + 2) % _w;
        int y0 = iv - 1, y1 = iv, y2 = (iv + 1) % _h, y3 = (iv + 2) % _h;
        if (x0 < 0) x0 = _w - 1;
        if (y0 < 0) y0 = _h - 1;

        Value                    a01 = get(x1, y0), a02 = get(x2, y0);
        Value a10 = get(x0, y1), a11 = get(x1, y1), a12 = get(x2, y1), a13 = get(x3, y1);
        Value a20 = get(x0, y2), a21 = get(x1, y2), a22 = get(x2, y2), a23 = get(x3, y2);
        Value                    a31 = get(x1, y3), a32 = get(x2, y3);

        Value du11 = a12 - a10, du12 = a13 - a11, du21 = a22 - a20, du22 = a23 - a21;
        Value dv11 = a21 - a01, dv21 = a31 - a11, dv12 = a22 - a02, dv22 = a32 - a12;

        derivs.x() = lerp(du11, du12, du21, du22, u, v)*float(_w);
        derivs.y() = lerp(dv11, dv12, dv21, dv22, u, v)*float(_h);
    }

    Value operator[](const Vec2f &uv) const override final
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

        return Value(lerp(
            get(iu, iv),
            get(iu + 1, iv),
            get(iu, iv + 1),
            get(iu + 1, iv + 1),
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

    bool isConstant() const
    {
        return false;
    }

    Value average() const override final
    {
        return _avg;
    }

    Value minimum() const override final
    {
        return _min;
    }

    Value maximum() const override final
    {
        return _max;
    }
};

template<bool Scalar, typename Texel>
class HdrBitmapTexture : public BitmapTexture<Scalar, 2>
{
    typedef typename Texture<Scalar, 2>::Value Value;

    using BitmapTexture<Scalar, 2>::_srcDir;
    using BitmapTexture<Scalar, 2>::_path;
    using BitmapTexture<Scalar, 2>::_w;
    using BitmapTexture<Scalar, 2>::_h;
    using BitmapTexture<Scalar, 2>::_min;
    using BitmapTexture<Scalar, 2>::_max;
    using BitmapTexture<Scalar, 2>::_avg;

    std::unique_ptr<Texel[]> _texels;

    bool loadPfm(const std::string &filename)
    {
        std::ifstream in(filename, std::ios_base::in | std::ios_base::binary);
        if (!in.good())
            return false;

        std::string ident;
        in >> ident;
        int channels;
        if (ident == "Pf")
            channels = 1;
        else if (ident == "PF")
            channels = 3;
        else
            return false;

        in >> _w >> _h;
        double s;
        in >> s;
        std::string tmp;
        std::getline(in, tmp);

        std::unique_ptr<float[]> img(new float[_w*_h*channels]);
        in.read(reinterpret_cast<char *>(img.get()), _w*_h*channels*sizeof(float));
        in.close();

        _texels.reset(new Value[_w*_h]);
        if ((channels == 1) == Scalar) {
            std::memcpy(_texels.get(), img.get(), _w*_h*sizeof(Value));
        } else {

            if (Scalar)
                for (int i = 0, idx = 0; i < _w*_h; ++i, idx += 3)
                    _texels[i] = Value((img[idx] + img[idx + 1] + img[idx + 2])*(1.0f/3.0f));
            else
                for (int i = 0; i < _w*_h; ++i)
                    _texels[i] = Value(img[i]);
        }

        Value minV(1e30f), maxV(-1e30f), avgV(0.0f);
        for (int i = 0; i < _w*_h; ++i) {
            minV = min(minV, _texels[i]);
            maxV = max(maxV, _texels[i]);
            avgV += _texels[i]/float(_w*_h);
        }
        _min = minV;
        _max = maxV;
        _avg = avgV;

        return true;
    }

protected:
    inline Value get(int x, int y) const override final
    {
        return _texels[x + y*_w];
    }

public:
    HdrBitmapTexture(const std::string &path)
    {
        _srcDir = FileUtils::getCurrentDir();
        _path = path;

        if (!loadPfm(path)) {
            _texels.reset(new Value[4]);
            _texels[0] = _texels[3] = Value(1.0f);
            _texels[1] = _texels[2] = Value(0.0f);
        }
    }
};

template<bool Scalar, typename Texel>
class LdrBitmapTexture : public BitmapTexture<Scalar, 2>
{
    typedef typename Texture<Scalar, 2>::Value Value;

    using BitmapTexture<Scalar, 2>::_srcDir;
    using BitmapTexture<Scalar, 2>::_path;
    using BitmapTexture<Scalar, 2>::_w;
    using BitmapTexture<Scalar, 2>::_h;
    using BitmapTexture<Scalar, 2>::_min;
    using BitmapTexture<Scalar, 2>::_max;
    using BitmapTexture<Scalar, 2>::_avg;

    Texel *_texels;
    int _channels;
    bool _invalid;

    Vec3f normalize(Rgba x) const
    {
        return Vec3f(
            x.c[0]*(1.0f/256.0f),
            x.c[1]*(1.0f/256.0f),
            x.c[2]*(1.0f/256.0f)
        );
    }

    Vec3f lerp(Rgba x00, Rgba x01, Rgba x10, Rgba x11, float u, float v) const
    {
        return (normalize(x00)*(1.0f - u) + normalize(x01)*u)*(1.0f - v) +
               (normalize(x10)*(1.0f - u) + normalize(x11)*u)*v;
    }

    float lerp(uint8 x00, uint8 x01, uint8 x10, uint8 x11, float u, float v) const
    {
        return ((x00*(1.0f/256.0f))*(1.0f - u) + (x01*(1.0f/256.0f))*u)*(1.0f - v) +
               ((x10*(1.0f/256.0f))*(1.0f - u) + (x11*(1.0f/256.0f))*u)*v;
    }

    float convert(uint8 c) const
    {
        return c*1.0f/256.0f;
    }

    Vec3f convert(Rgba x) const
    {
        return normalize(x);
    }

protected:
    inline Value get(int x, int y) const override final
    {
        return convert(_texels[x + y*_w]);
    }

public:
    explicit LdrBitmapTexture(const std::string &path)
    : _invalid(true)
    {
        _srcDir = FileUtils::getCurrentDir();
        _path = path;

        if (Scalar) {
            uint8 *tmp = stbi_load(path.c_str(), &_w, &_h, &_channels, 0);

            if (_channels == 4) {
                _texels = reinterpret_cast<Texel *>(tmp);
                for (size_t i = 0; i < _w*_h; ++i)
                    _texels[i] = Texel(tmp[i*4 + 3]);
            } else {
                stbi_image_free(tmp);
                _texels = reinterpret_cast<Texel *>(stbi_load(path.c_str(), &_w, &_h, &_channels, 1));
            }
        } else
            _texels = reinterpret_cast<Texel *>(stbi_load(path.c_str(), &_w, &_h, &_channels, 4));

        if (!_texels) {
            DBG("Unable to load texture at '%s' (current dir '%s')", path, FileUtils::getCurrentDir());
            _invalid = true;
            _texels = new Texel[2*2];
            _w = 2;
            _h = 2;
            _channels = Scalar ? 1 : 3;
            _texels[0] = _texels[3] = Texel(255);
            _texels[1] = _texels[2] = Texel(0);
        } else {
            Texel minT(_texels[0]);
            Texel maxT(_texels[0]);
            Value avgT(convert(_texels[0]));

            for (size_t i = 1; i < _w*_h; ++i) {
                minT = min(minT, _texels[i]);
                maxT = max(maxT, _texels[i]);
                avgT += convert(_texels[i]);
            }

            _min = convert(minT);
            _max = convert(maxT);
            _avg = avgT/Value(float(_w*_h));
        }
    }

    ~LdrBitmapTexture()
    {
        if (_invalid)
            delete[] _texels;
        else
            stbi_image_free(_texels);
    }
};

namespace BitmapTextureUtils
{

inline std::shared_ptr<BitmapTexture<false, 2>> loadColorTexture(const std::string &path)
{
    std::string extension = FileUtils::extractExt(path);
    if (extension == "pfm")
        return std::make_shared<HdrBitmapTexture<false, Vec3f>>(path);
    else
        return std::make_shared<LdrBitmapTexture<false, Rgba>>(path);
}

inline std::shared_ptr<BitmapTexture<true, 2>> loadScalarTexture(const std::string &path)
{
    std::string extension = FileUtils::extractExt(path);
    if (extension == "pfm")
        return std::make_shared<HdrBitmapTexture<true, float>>(path);
    else
        return std::make_shared<LdrBitmapTexture<true, uint8>>(path);
}

}

typedef BitmapTexture<true, 2> BitmapTextureA;
typedef BitmapTexture<false, 2> BitmapTextureRgb;

}

#endif /* BITMAPTEXTURE_HPP_ */
