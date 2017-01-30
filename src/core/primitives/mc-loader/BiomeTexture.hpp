#ifndef BIOMETEXTURE_HPP_
#define BIOMETEXTURE_HPP_

#include "textures/BitmapTexture.hpp"
#include "textures/Texture.hpp"

#include <unordered_map>
#include <memory>

namespace Tungsten {
namespace MinecraftLoader {

struct BiomeTileTexture;

class BiomeTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    std::shared_ptr<BitmapTexture> _substrate;
    std::shared_ptr<BitmapTexture> _overlay;
    std::shared_ptr<BitmapTexture> _overlayOpacity;
    const std::unordered_map<Vec2i, const BiomeTileTexture *> &_biomes;
    int _tintType;

public:
    BiomeTexture(std::shared_ptr<BitmapTexture> substrate,
            std::shared_ptr<BitmapTexture> overlay,
            std::shared_ptr<BitmapTexture> overlayOpacity,
            const std::unordered_map<Vec2i, const BiomeTileTexture *> &biomes,
            int tintType)
    : _substrate(substrate),
      _overlay(overlay),
      _overlayOpacity(overlayOpacity),
      _biomes(biomes),
      _tintType(tintType)
    {
    }

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec2f &uv) const override;
    virtual Vec3f operator[](const IntersectionInfo &info) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const override;

    virtual void scaleValues(float factor) override;

    virtual Texture *clone() const override;
};

}
}

#endif /* BIOMETEXTURE_HPP_ */
