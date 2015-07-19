#include "MixedBsdf.hpp"
#include "ErrorBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <rapidjson/document.h>

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

void MixedBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);

    _bsdf0 = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf0"));
    _bsdf1 = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf1"));
    if (_bsdf0.get() == this || _bsdf1.get() == this) {
        DBG("Warning: Recursive mixed BSDF not supported");
        _bsdf0 = scene.errorBsdf();
        _bsdf1 = scene.errorBsdf();
    }
    scene.textureFromJsonMember(v, "ratio", TexelConversion::REQUEST_AVERAGE, _ratio);
}

rapidjson::Value MixedBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "mixed", allocator);
    JsonUtils::addObjectMember(v, "bsdf0", *_bsdf0, allocator);
    JsonUtils::addObjectMember(v, "bsdf1", *_bsdf1, allocator);
    JsonUtils::addObjectMember(v, "ratio", *_ratio, allocator);
    return std::move(v);
}

bool MixedBsdf::sample(SurfaceScatterEvent &event) const
{
    float ratio;
    if (!adjustedRatio(event.requestedLobe, event.info, ratio))
        return false;

    if (event.sampler->nextBoolean(DiscreteBsdfSample, ratio)) {
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


