#ifndef BITMAPTEXTURE_HPP_
#define BITMAPTEXTURE_HPP_

#include "Texture.hpp"

#include "io/ImageIO.hpp"
#include "io/Path.hpp"

namespace Tungsten {

class Distribution2D;

class BitmapTexture : public Texture
{
public:
    enum class TexelType : uint32 {
        SCALAR_LDR = 0,
        SCALAR_HDR = 1,
        RGB_LDR    = 2,
        RGB_HDR    = 3,
    };

private:
    typedef JsonSerializable::Allocator Allocator;

    PathPtr _path;
    TexelConversion _texelConversion;
    bool _gammaCorrect;
    bool _linear, _clamp;
    bool _valid;

    Vec3f _min, _max, _avg;
    void *_texels;
    int _w;
    int _h;
    TexelType _texelType;
    float _scale;

    std::unique_ptr<Distribution2D> _distribution[MAP_JACOBIAN_COUNT];

    inline bool isRgb() const;
    inline bool isHdr() const;

    inline float lerp(float x00, float x01, float x10, float x11, float u, float v) const;
    inline Vec3f lerp(Vec3f x00, Vec3f x01, Vec3f x10, Vec3f x11, float u, float v) const;

    template<typename T>
    inline const T *as() const;

    inline float getScalar(int x, int y) const;
    inline Vec3f getRgb(int x, int y) const;
    inline float weight(int x, int y) const;

protected:
    TexelType getTexelType(bool isRgb, bool isHdr);

    void init(void *texels, int w, int h, TexelType texelType);

public:
    BitmapTexture();
    BitmapTexture(const Path &path, TexelConversion conversion, bool gammaCorrect, bool linear, bool clamp);
    BitmapTexture(PathPtr path, TexelConversion conversion, bool gammaCorrect, bool linear, bool clamp);
    BitmapTexture(void *texels, int w, int h, TexelType texelType, bool linear, bool clamp);

    BitmapTexture(const BitmapTexture &o);

    ~BitmapTexture();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec2f &uv) const override final;
    virtual Vec3f operator[](const IntersectionInfo &info) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual Vec2f invert(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const override;

    virtual void scaleValues(float factor) override;

    virtual Texture *clone() const override;

    const PathPtr &path() const
    {
        return _path;
    }

    int w() const
    {
        return _w;
    }

    int h() const
    {
        return _h;
    }

    bool isValid() const
    {
        return _valid;
    }

    bool clamp() const
    {
        return _clamp;
    }

    bool gammaCorrect() const
    {
        return _gammaCorrect;
    }

    bool linear() const
    {
        return _linear;
    }

    void setClamp(bool clamp)
    {
        _clamp = clamp;
    }

    void setGammaCorrect(bool gammaCorrect)
    {
        _gammaCorrect = gammaCorrect;
    }

    void setLinear(bool linear)
    {
        _linear = linear;
    }

    TexelConversion texelConversion() const
    {
        return _texelConversion;
    }

    void setTexelConversion(TexelConversion conversion)
    {
        _texelConversion = conversion;
    }

    bool operator<(const BitmapTexture &o) const
    {
        return
            _path != o._path ? _path < o._path :
            _texelConversion != o._texelConversion ? _texelConversion < o._texelConversion :
            _gammaCorrect != o._gammaCorrect ? _gammaCorrect < o._gammaCorrect :
            _linear != o._linear ? _linear < o._linear :
            _clamp != o._clamp ? _clamp < o._clamp :
            false;

    }

    bool operator==(const BitmapTexture &o) const
    {
        return
            _path == o._path &&
            _texelConversion == o._texelConversion &&
            _gammaCorrect == o._gammaCorrect &&
            _linear == o._linear &&
            _clamp == o._clamp;
    }
};

}

#endif /* BITMAPTEXTURE_HPP_ */
