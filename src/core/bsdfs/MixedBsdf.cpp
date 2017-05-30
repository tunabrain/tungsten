#include "MixedBsdf.hpp"
#include "ErrorBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Vec.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

bool MixedBsdf::adjustedRatio(BsdfLobes requestedLobe, const IntersectionInfo *info, float &ratio) const
{
    bool sample0 = requestedLobe.test(_bsdf0->lobes());
    bool sample1 = requestedLobe.test(_bsdf1->lobes());

    if (sample0 && sample1)
        ratio = (*_ratio)[*info].x();
    else if (sample0)
        ratio = 1.0f;
    else if (sample1)
        ratio = 0.0f;
    else
        return false;
    return true;
}

MixedBsdf::MixedBsdf()
: _bsdf0(std::make_shared<ErrorBsdf>()),
  _bsdf1(_bsdf0),
  _ratio(std::make_shared<ConstantTexture>(0.5f))
{
}

MixedBsdf::MixedBsdf(std::shared_ptr<Bsdf> bsdf0, std::shared_ptr<Bsdf> bsdf1, float ratio)
: _bsdf0(bsdf0),
  _bsdf1(bsdf1),
  _ratio(std::make_shared<ConstantTexture>(ratio))
{
    _lobes = BsdfLobes(_bsdf0->lobes(), _bsdf1->lobes());
}

void MixedBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);

    _bsdf0 = scene.fetchBsdf(value.getRequiredMember("bsdf0"));
    _bsdf1 = scene.fetchBsdf(value.getRequiredMember("bsdf1"));
    if (_bsdf0.get() == this || _bsdf1.get() == this)
        value.parseError("Recursive mixed BSDF not supported");
    if (auto ratio = value["ratio"])
        _ratio = scene.fetchTexture(ratio, TexelConversion::REQUEST_AVERAGE);
}

rapidjson::Value MixedBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "mixed",
        "bsdf0", *_bsdf0,
        "bsdf1", *_bsdf1,
        "ratio", *_ratio
    };
}

bool MixedBsdf::sample(SurfaceScatterEvent &event) const
{
    float ratio;
    if (!adjustedRatio(event.requestedLobe, event.info, ratio))
        return false;

    if (event.sampler->nextBoolean(ratio)) {
        if (!_bsdf0->sample(event))
            return false;

        float pdf0 = event.pdf*ratio;
        float pdf1 = _bsdf1->pdf(event)*(1.0f - ratio);
        Vec3f f = event.weight*event.pdf*ratio + _bsdf1->eval(event)*(1.0f - ratio);
        event.pdf = pdf0 + pdf1;
        event.weight = f/event.pdf;
    } else {
        if (!_bsdf1->sample(event))
            return false;

        float pdf0 = _bsdf0->pdf(event)*ratio;
        float pdf1 = event.pdf*(1.0f - ratio);
        Vec3f f = _bsdf0->eval(event)*ratio + event.weight*event.pdf*(1.0f - ratio);
        event.pdf = pdf0 + pdf1;
        event.weight = f/event.pdf;
    }

    event.weight *= albedo(event.info);
    return true;
}

Vec3f MixedBsdf::eval(const SurfaceScatterEvent &event) const
{
    float ratio = (*_ratio)[*event.info].x();
    return albedo(event.info)*(_bsdf0->eval(event)*ratio + _bsdf1->eval(event)*(1.0f - ratio));
}

bool MixedBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    float ratio;
    if (!adjustedRatio(event.requestedLobe, event.info, ratio))
        return false;

    float pdf0 = _bsdf0->pdf(event)*ratio;
    float pdf1 = _bsdf1->pdf(event)*(1.0f - ratio);

    if (sampler.untrackedBoolean(pdf0/(pdf0 + pdf1))) {
        sampler.putBoolean(ratio, true);
        return _bsdf0->invert(sampler, event);
    } else {
        sampler.putBoolean(ratio, false);
        return _bsdf1->invert(sampler, event);
    }
}

float MixedBsdf::pdf(const SurfaceScatterEvent &event) const
{
    float ratio;
    if (!adjustedRatio(event.requestedLobe, event.info, ratio))
        return 0.0f;
    return _bsdf0->pdf(event)*ratio + _bsdf1->pdf(event)*(1.0f - ratio);
}

void MixedBsdf::prepareForRender()
{
    _lobes = BsdfLobes(_bsdf0->lobes(), _bsdf1->lobes());
}

}


