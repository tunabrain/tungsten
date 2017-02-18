#ifndef BSDF_HPP_
#define BSDF_HPP_

#include "BsdfLobes.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "primitives/IntersectionInfo.hpp"

#include "textures/Texture.hpp"

#include "sampling/WritablePathSampleGenerator.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

#include <rapidjson/document.h>
#include <memory>

namespace Tungsten {

class Medium;

static CONSTEXPR float DiracAcceptanceThreshold = 1e-3f;

class Bsdf : public JsonSerializable
{
    static std::shared_ptr<Texture> _defaultAlbedo;

protected:

    BsdfLobes _lobes;

    std::shared_ptr<Texture> _albedo;
    std::shared_ptr<Texture> _bump;

    Vec3f albedo(const IntersectionInfo *info) const
    {
        return (*_albedo)[*info];
    }

    static inline bool checkReflectionConstraint(const Vec3f &wi, const Vec3f &wo)
    {
        return std::abs(wi.z()*wo.z() - wi.x()*wo.x() - wi.y()*wo.y() - 1.0f) < DiracAcceptanceThreshold;
    }

    static inline bool checkRefractionConstraint(const Vec3f &wi, const Vec3f &wo, float eta, float cosThetaT)
    {
        float dotP = -wi.x()*wo.x()*eta - wi.y()*wo.y()*eta - std::copysign(cosThetaT, wi.z())*wo.z();
        return std::abs(dotP - 1.0f) < DiracAcceptanceThreshold;
    }

public:
    virtual ~Bsdf()
    {
    }

    Bsdf();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f eval(const SurfaceScatterEvent &event) const = 0;
    virtual bool sample(SurfaceScatterEvent &event) const = 0;
    virtual bool invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const;
    virtual float pdf(const SurfaceScatterEvent &event) const = 0;

    inline bool sample(SurfaceScatterEvent &event, bool adjoint) const
    {
        if (!sample(event))
            return false;

        if (adjoint)
            event.weight *= std::abs(
                (event.frame.toGlobal(event.wo).dot(event.info->Ng)*event.wi.z())/
                (event.frame.toGlobal(event.wi).dot(event.info->Ng)*event.wo.z())); // TODO: Optimize
        else
            event.weight *= sqr(eta(event));

        return true;
    }
    inline Vec3f eval(const SurfaceScatterEvent &event, bool adjoint) const
    {
        Vec3f f = eval(event);

        if (adjoint)
            f *= std::abs(
                 (event.frame.toGlobal(event.wo).dot(event.info->Ng)*event.wi.z())/
                 (event.frame.toGlobal(event.wi).dot(event.info->Ng)*event.wo.z())); // TODO: Optimize
        else
            f *= sqr(eta(event));

        return f;
    }

    // Returns etaI/etaO
    virtual float eta(const SurfaceScatterEvent &/*event*/) const
    {
        return 1.0f;
    }

    virtual void prepareForRender() {}
    virtual void teardownAfterRender() {}

    const BsdfLobes &lobes() const
    {
        return _lobes;
    }

    void setAlbedo(const std::shared_ptr<Texture> &c)
    {
        _albedo = c;
    }

    std::shared_ptr<Texture> &albedo()
    {
        return _albedo;
    }

    const std::shared_ptr<Texture> &albedo() const
    {
        return _albedo;
    }

    void setBump(const std::shared_ptr<Texture> &b)
    {
        _bump = b;
    }

    std::shared_ptr<Texture> &bump()
    {
        return _bump;
    }

    const std::shared_ptr<Texture> &bump() const
    {
        return _bump;
    }
};

}

#endif /* BSDF_HPP_ */
