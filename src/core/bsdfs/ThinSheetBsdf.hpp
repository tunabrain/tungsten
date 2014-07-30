#ifndef THINSHEETBSDF_HPP_
#define THINSHEETBSDF_HPP_

#include <rapidjson/document.h>

#include "Bsdf.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SurfaceScatterEvent.hpp"
#include "sampling/Sample.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

namespace Tungsten
{

class Scene;

class ThinSheetBsdf : public Bsdf
{
    static constexpr bool UseAlphaTrick = true;

    float _ior;
    std::shared_ptr<TextureA> _thickness;
    Vec3f _sigmaA;

public:
    ThinSheetBsdf()
    : _ior(1.5f),
      _thickness(std::make_shared<ConstantTextureA>(0.5f)),
      _sigmaA(0.0f)
    {
        _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Bsdf::fromJson(v, scene);
        JsonUtils::fromJson(v, "ior", _ior);
        JsonUtils::fromJson(v, "sigmaA", _sigmaA);

        const rapidjson::Value::Member *thickness = v.FindMember("thickness");
        if (thickness)
            _thickness = scene.fetchScalarTexture<2>(thickness->value);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Bsdf::toJson(allocator);
        v.AddMember("type", "thinsheet", allocator);
        v.AddMember("ior", _ior, allocator);
        JsonUtils::addObjectMember(v, "thickness", *_thickness, allocator);
        v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
        return std::move(v);
    }

    virtual float alpha(const IntersectionInfo *info) const override final
    {
        if (!UseAlphaTrick) {
            return 1.0f;
        } else {
            float thickness = (*_thickness)[info->uv]*500.0f;
            float R, cosThetaT;
            Vec3f interference = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                    std::abs(info->w.dot(info->Ns)), thickness, R, cosThetaT);
            return interference.avg();
//          float cosThetaT;
//          return Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), cosThetaT);
        }
    }

    virtual Vec3f transmittance(const IntersectionInfo *info) const override final
    {
//      return Vec3f(1.0f);
//      float thickness = (*_thickness)[info->uv];
//      if (_sigmaA == 0.0f || thickness == 0.0f)
//          return Vec3f(1.0f);
//
//      float cosThetaT;
//      float R = Fresnel::dielectricReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), cosThetaT);
//
//      Vec3f absorption = std::exp(-_sigmaA*thickness/cosThetaT);
//      Vec3f interference = Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), thickness);
//
//      return absorption*(1.0f - R)*(1.0f - R)/(1.0f - absorption*R*R);
//      return Vec3f(1.0f);
//      if (/*_sigmaA == 0.0f || */thickness == 0.0f)
//          return Vec3f(1.0f);
//
        float thickness = (*_thickness)[info->uv]*500.0f;
        float R, cosThetaT;
        Vec3f interference = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(info->w.dot(info->Ns)), thickness, R, cosThetaT);
        return (1.0f - interference)/(1.0f - interference.avg());
//
//      return ((1.0f - interference)*(1.0f + R))/((1.0f + interference)*(1.0f - R));

        //Vec3f absorption = std::exp(-_sigmaA*thickness/cosThetaT);

        //return absorption*(1.0f - R)*(1.0f - R)/(1.0f - absorption*R*R);
    }

    bool sample(SurfaceScatterEvent &event) const override final
    {
        if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
            return false;
        if (!UseAlphaTrick) {
            float cosThetaT;
            float R = Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(event.wi.z()), cosThetaT);

            if (event.sampler->next1D() < R) {
                event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
                event.pdf = R;
                event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
            } else {
                event.wo = Vec3f(-event.wi.x(), -event.wi.y(), -event.wi.z());
                event.pdf = 1.0f - R;
                event.sampledLobe = BsdfLobes::SpecularTransmissionLobe;
            }
            event.throughput = Vec3f(1.0f);
        } else {
            event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
            event.pdf = 1.0f;
            event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
//          event.throughput = Vec3f(1.0f);
            float R, cosThetaT;
            event.throughput = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                    std::abs(event.wi.z()), (*_thickness)[event.info->uv]*500.0f, R, cosThetaT);
            event.throughput /= event.throughput.avg();
        }
        return true;
    }

    Vec3f eval(const SurfaceScatterEvent &/*event*/) const override final
    {
        return Vec3f(0.0f);
    }

    float pdf(const SurfaceScatterEvent &/*event*/) const override final
    {
        return 0.0f;
    }

    float ior() const
    {
        return _ior;
    }
};

}


#endif /* THINSHEETBSDF_HPP_ */
