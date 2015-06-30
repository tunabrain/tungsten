#ifndef LAMBERTIANFIBERBCSDF_HPP_
#define LAMBERTIANFIBERBCSDF_HPP_

#include "bsdfs/Bsdf.hpp"

namespace Tungsten {

class LambertianFiberBcsdf : public Bsdf
{
    inline float lambertianCylinder(const Vec3f &wo) const;

public:
    LambertianFiberBcsdf();

    rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const SurfaceScatterEvent &event) const override;
    virtual bool sample(SurfaceScatterEvent &event) const override;
    virtual float pdf(const SurfaceScatterEvent &event) const override;
};

}

#endif /* LAMBERTIANFIBERBCSDF_HPP_ */
