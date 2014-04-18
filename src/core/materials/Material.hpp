#ifndef MATERIAL_HPP_
#define MATERIAL_HPP_

#include <rapidjson/document.h>
#include <memory>
#include <string>

#include "Texture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include "Debug.hpp"

namespace Tungsten
{

class Scene;

class Material : public JsonSerializable
{
    std::shared_ptr<Bsdf> _bsdf;
    Vec3f _emission;

    std::shared_ptr<TextureRgba> _base;
    std::shared_ptr<TextureA> _alpha;
    std::shared_ptr<TextureA> _bump;

public:
    Material(const rapidjson::Value &v, const Scene &scene);
    Material(std::shared_ptr<Bsdf> bsdf, const Vec3f &emission, const std::string &name,
            std::shared_ptr<TextureRgba> base = nullptr,
            std::shared_ptr<TextureA> alpha = nullptr,
            std::shared_ptr<TextureA> bump = nullptr);

    rapidjson::Value toJson(Allocator &allocator) const;

    std::shared_ptr<Bsdf> &bsdf()
    {
        return _bsdf;
    }

    const std::shared_ptr<Bsdf> &bsdf() const
    {
        return _bsdf;
    }

    const Vec3f &emission() const
    {
        return _emission;
    }

    void setColor(std::shared_ptr<TextureRgba> &c)
    {
        _base = c;
    }

    void setAlpha(std::shared_ptr<TextureA> &a)
    {
        _alpha = a;
    }

    void setBump(std::shared_ptr<TextureA> &b)
    {
        _bump = b;
    }

    std::shared_ptr<TextureRgba> &color()
    {
        return _base;
    }

    std::shared_ptr<TextureA> &alpha()
    {
        return _alpha;
    }

    std::shared_ptr<TextureA> &bump()
    {
        return _bump;
    }

    const std::shared_ptr<TextureRgba> &color() const
    {
        return _base;
    }

    const std::shared_ptr<TextureA> &alpha() const
    {
        return _alpha;
    }

    const std::shared_ptr<TextureA> &bump() const
    {
        return _bump;
    }

    Vec3f color(const Vec2f &uv) const
    {
        if (_base)
            return (*_base)[uv].xyz();
        else
            return Vec3f(1.0f);
    }

    float alpha(const Vec2f &uv) const
    {
        if (_alpha)
            return (*_alpha)[uv];
        else
            return 1.0f;
    }

    float bump(const Vec2f &uv) const
    {
        if (_bump)
            return (*_bump)[uv]*2.0f - 1.0f;
        else
            return 0.0f;
    }
};

}

#endif /* MATERIAL_HPP_ */
