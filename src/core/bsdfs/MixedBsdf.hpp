#ifndef MIXEDBSDF_HPP_
#define MIXEDBSDF_HPP_

#include <rapidjson/document.h>
#include <memory>

#include "Bsdf.hpp"

#include "sampling/ScatterEvent.hpp"

#include "math/Vec.hpp"

namespace Tungsten
{

class Scene;

class MixedBsdf : public Bsdf
{
    std::shared_ptr<Bsdf> _bsdf0, _bsdf1;
    float _ratio;

public:
    MixedBsdf(const rapidjson::Value &v, const Scene &scene);

    MixedBsdf(std::shared_ptr<Bsdf> bsdf0, std::shared_ptr<Bsdf> bsdf1, float ratio)
    : _bsdf0(bsdf0),
      _bsdf1(bsdf1),
      _ratio(ratio)
    {
        _flags = BsdfFlags(_bsdf0->flags(), _bsdf1->flags());
    }

    rapidjson::Value toJson(Allocator &allocator) const;

    void chooseBrdf(Bsdf *&bsdf, float &sample) const
    {
        if (sample < _ratio) {
            sample /= _ratio;
            bsdf = _bsdf0.get();
        } else {
            sample = (sample - _ratio)/(1.0f - _ratio);
            bsdf = _bsdf1.get();
        }
    }

    bool sample(ScatterEvent &event) const final override
    {
        Bsdf *bsdf;
        chooseBrdf(bsdf, event.xi.z());

        return bsdf->sample(event);
    }

    bool sampleFromLight(ScatterEvent &event) const final override
    {
        Bsdf *bsdf;
        chooseBrdf(bsdf, event.xi.z());

        return bsdf->sampleFromLight(event);
    }

    Vec3f eval(const Vec3f &wo, const Vec3f &wi) const final override
    {
        return _bsdf0->eval(wo, wi)*_ratio + _bsdf1->eval(wo, wi)*(1.0f - _ratio);
    }

    float pdf(const Vec3f &wo, const Vec3f &wi) const final override
    {
        return _bsdf0->pdf(wo, wi)*_ratio + _bsdf1->pdf(wo, wi)*(1.0f - _ratio);
    }

    float pdfFromLight(const Vec3f &wo, const Vec3f &wi) const final override
    {
        return _bsdf0->pdfFromLight(wo, wi)*_ratio + _bsdf1->pdfFromLight(wo, wi)*(1.0f - _ratio);
    }
};

}

#endif /* MIXEDBSDF_HPP_ */
