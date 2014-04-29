#ifndef MIXEDBSDF_HPP_
#define MIXEDBSDF_HPP_

#include <rapidjson/document.h>
#include <memory>

#include "Bsdf.hpp"
#include "ErrorBsdf.hpp"

#include "sampling/ScatterEvent.hpp"

#include "math/Vec.hpp"

namespace Tungsten
{

class Scene;

class MixedBsdf : public Bsdf
{
    std::shared_ptr<Bsdf> _bsdf0, _bsdf1;
    float _ratio;

    bool adjustedRatio(BsdfLobes requestedLobe, float &ratio) const
    {
        bool sample0 = requestedLobe.test(_bsdf0->flags());
        bool sample1 = requestedLobe.test(_bsdf1->flags());

        if (sample0 && sample1)
            ratio = _ratio;
        else if (sample0)
            ratio = 1.0f;
        else if (sample1)
            ratio = 0.0f;
        else
            return false;
        return true;
    }

public:
    MixedBsdf()
    : _bsdf0(std::make_shared<ErrorBsdf>()),
      _bsdf1(_bsdf0),
      _ratio(0.5f)
    {

    }

    MixedBsdf(std::shared_ptr<Bsdf> bsdf0, std::shared_ptr<Bsdf> bsdf1, float ratio)
    : _bsdf0(bsdf0),
      _bsdf1(bsdf1),
      _ratio(ratio)
    {
        _lobes = BsdfLobes(_bsdf0->flags(), _bsdf1->flags());
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual float alpha(const IntersectionInfo &info) const
    {
        return (*_alpha)[info.uv]*(_bsdf0->alpha(info)*_ratio + _bsdf1->alpha(info)*(1.0f - _ratio));
    }

    bool sample(SurfaceScatterEvent &event) const final override
    {
        float ratio;
        if (!adjustedRatio(event.requestedLobe, ratio))
            return false;

        if (event.supplementalSampler.next1D() < ratio) {
            if (!_bsdf0->sample(event))
                return false;

            float pdf0 = event.pdf*ratio;
            float pdf1 = _bsdf1->pdf(event)*(1.0f - ratio);
            Vec3f f = event.throughput*event.pdf*ratio + _bsdf1->eval(event)*(1.0f - ratio);
            event.throughput = f/pdf0*Sample::powerHeuristic(pdf0, pdf1);
            event.pdf = pdf0 + pdf1;
        } else {
            if (!_bsdf1->sample(event))
                return false;

            float pdf0 = _bsdf1->pdf(event)*ratio;
            float pdf1 = event.pdf*(1.0f - ratio);
            Vec3f f = _bsdf0->eval(event)*ratio + event.throughput*event.pdf*(1.0f - ratio);
            event.throughput = f/pdf1*Sample::powerHeuristic(pdf1, pdf0);
            event.pdf = pdf0 + pdf1;
        }

        event.throughput *= base(event.info);
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &event) const final override
    {
        float ratio;
        if (!adjustedRatio(event.requestedLobe, ratio))
            return Vec3f(0.0f);
        return base(event.info)*(_bsdf0->eval(event)*ratio + _bsdf1->eval(event)*(1.0f - ratio));
    }

    float pdf(const SurfaceScatterEvent &event) const final override
    {
        float ratio;
        if (!adjustedRatio(event.requestedLobe, ratio))
            return 0.0f;
        return _bsdf0->pdf(event)*ratio + _bsdf1->pdf(event)*(1.0f - ratio);
    }

    const std::shared_ptr<Bsdf> &bsdf0() const
    {
        return _bsdf0;
    }

    const std::shared_ptr<Bsdf> &bsdf1() const
    {
        return _bsdf1;
    }

    float ratio() const
    {
        return _ratio;
    }
};

}

#endif /* MIXEDBSDF_HPP_ */
