#ifndef TRANSPARENCYBSDF_HPP_
#define TRANSPARENCYBSDF_HPP_

#include "Bsdf.hpp"

namespace Tungsten {

class TransparencyBsdf : public Bsdf
{
    std::shared_ptr<Texture> _opacity;
    std::shared_ptr<Bsdf> _base;

public:
    TransparencyBsdf();
    TransparencyBsdf(std::shared_ptr<Texture> opacity, std::shared_ptr<Bsdf> base);

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    rapidjson::Value toJson(Allocator &allocator) const override;

    bool sample(SurfaceScatterEvent &event) const final override;
    Vec3f eval(const SurfaceScatterEvent &event) const final override;
    float pdf(const SurfaceScatterEvent &event) const final override;

    const std::shared_ptr<Texture> &opacity() const
    {
        return _opacity;
    }

    const std::shared_ptr<Bsdf> &base() const
    {
        return _base;
    }
};

}


#endif /* TRANSPARENCYBSDF_HPP_ */
