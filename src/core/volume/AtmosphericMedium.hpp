#ifndef ATMOSPHERICMEDIUM_HPP_
#define ATMOSPHERICMEDIUM_HPP_

#include "Medium.hpp"

#include "primitives/Primitive.hpp"

namespace Tungsten {

class AtmosphericMedium : public Medium
{
    static constexpr float Rg = 6360.0f*1e3f;
    static constexpr float Rt = 6420.0f*1e3f;
    static constexpr float Hr = 8.0f*1e3f;
    static constexpr float BetaR = 5.8f*1e-6f;
    static constexpr float BetaG = 13.5f*1e-6f;
    static constexpr float BetaB = 33.1f*1e-6f;
    static constexpr float FalloffScale = 14.0f;

    const Scene *_scene;

    std::string _primName;

    Vec3f _pos;
    float _scale;
    float _cloudMinR;
    float _cloudMaxR;
    float _cloudDensity;
    float _cloudAlbedo;
    float _cloudShift;
    Vec3f _backgroundSigmaS;
    std::shared_ptr<Texture> _cloudThickness;

    Vec3f _sigmaS;
    float _rG;
    float _rT;
    float _hR;

    inline bool sphereMinTMaxT(const Vec3f &o, const Vec3f &w, Vec2f &tSpan, float r) const
    {
        Vec3f p = o - _pos;
        float B = p.dot(w);
        float C = p.lengthSq() - r*r;
        float detSq = B*B - C;
        if (detSq >= 0.0f) {
            float det = std::sqrt(detSq);
            tSpan.x() = -B - det;
            tSpan.y() = -B + det;
            return true;
        }

        return false;
    }

    bool useCloudDensity(const Vec3f &q, float d) const
    {
        Vec2f uv(std::atan2(q.y(), q.x())*INV_TWO_PI + 0.5f, std::acos(clamp(q.z(), -1.0f, 1.0f))*INV_PI);
        if (std::isnan(uv.x()))
            uv.x() = 0.0f;
        uv.x() += _cloudShift;

        float cloudThickness = (*_cloudThickness)[uv].x();
        return (cloudThickness*(_cloudMaxR - _cloudMinR) > d - _rG - _cloudMinR);
    }

    Vec3f lerpSigma(const Vec3f &a, const Vec3f &b, float r) const
    {
        return a*(1.0f - r) + b*r;
    }

    Vec4f spectralOpticalDepthAndT(const Vec3f &p, const Vec3f &w, float maxT, float targetDepth, int targetChannel, float rand) const
    {
        constexpr int MaxStepCount = 1024*10;
        constexpr float maxError = 0.02f;
        int iter = 0;

        Vec3f localP = p - _pos;
        float r = localP.length();
        float mu = w.dot(localP)/r;

        Vec3f opticalDepth(0.0f);
        Vec2f dD = densityAndDerivative(r, mu, 0.0f, r);
        Vec3f sigmaS = lerpSigma(_backgroundSigmaS, _sigmaS, dD.x());
        float dT = min(maxError/dD.y(), maxT*0.2f);
        float t = dT*rand;
        while (t < maxT) {
            float d = std::sqrt(r*r + t*t + 2.0f*r*t*mu);

            Vec2f newDd = densityAndDerivative(r, mu, t, d);
            float newDt = min(maxError/newDd.y(), maxT*0.2f);
            Vec3f newSigmaS = lerpSigma(_backgroundSigmaS, _sigmaS, newDd.x());

            bool insideClouds = (d - _rG >= _cloudMinR && d - _rG <= _cloudMaxR);
            if (insideClouds) {
                newDt = (_cloudMaxR - _cloudMinR)*0.1f;
                if (useCloudDensity((localP + w*t)/d, d))
                    newSigmaS = Vec3f(_cloudDensity);
            }

            Vec3f depthStep = (sigmaS + newSigmaS)*0.5f*dT;
            opticalDepth += depthStep;
            if (opticalDepth[targetChannel] >= targetDepth) {
                float alpha = (targetDepth - opticalDepth[targetChannel] + depthStep[targetChannel])/depthStep[targetChannel];
                t -= dT*(1.0f - alpha);
                return Vec4f(1.0f, 1.0f, 1.0f, t);
            }

            dD = newDd;
            dT = newDt;
            sigmaS = newSigmaS;
            t += dT;

            if (iter++ > MaxStepCount) {
                FAIL("Exceeded maximum step count: %s %s %s %s", p, w, maxT, targetDepth);
            }
        }
        float d = std::sqrt(r*r + maxT*maxT + 2.0f*r*maxT*mu);
        Vec2f newDd = densityAndDerivative(r, mu, maxT, d);
        Vec3f newSigmaS = lerpSigma(_backgroundSigmaS, _sigmaS, newDd.x());
        opticalDepth += (sigmaS + newSigmaS)*0.5f*(maxT - t + dT);

        return Vec4f(opticalDepth.x(), opticalDepth.y(), opticalDepth.z(), maxT);
    }

    Vec2f opticalDepthAndT(const Vec3f &p, const Vec3f &w, float maxT, float targetDepth) const
    {
        constexpr int MaxStepCount = 1024*10;
        constexpr float maxError = 0.02f;
        int iter = 0;

        Vec3f localP = p - _pos;
        float r = localP.length();
        float mu = w.dot(localP)/r;

        float opticalDepth = 0.0f;
        Vec2f dD = densityAndDerivative(r, mu, 0.0f, r);
        float dT = min(maxError/dD.y(), maxT*0.2f);
        float t = dT;
        while (t < maxT) {
            float d = std::sqrt(r*r + t*t + 2.0f*r*t*mu);
            Vec2f newDd = densityAndDerivative(r, mu, t, d);

            float depthStep = (dD.x() + newDd.x())*0.5f*dT;
            opticalDepth += depthStep;
            if (opticalDepth >= targetDepth) {
                float alpha = (targetDepth - opticalDepth + depthStep)/depthStep;
                t -= dT*(1.0f - alpha);
                return Vec2f(targetDepth, t);
            }

            dD = newDd;
            dT = min(maxError/newDd.y(), maxT*0.2f);
            t += dT;

            if (iter++ > MaxStepCount) {
                FAIL("Exceeded maximum step count: %s %s %s %s", p, w, maxT, targetDepth);
            }
        }
        float d = std::sqrt(r*r + maxT*maxT + 2.0f*r*maxT*mu);
        Vec2f newDd = densityAndDerivative(r, mu, maxT, d);
        opticalDepth += (dD.x() + newDd.x())*0.5f*(maxT - t + dT);

        return Vec2f(opticalDepth, maxT);
    }

public:
    AtmosphericMedium()
    : _scene(nullptr),
      _pos(0.0f),
      _scale(Rg),
      _cloudDensity(1.0f),
      _cloudAlbedo(0.9f),
      _cloudShift(0.0f),
      _backgroundSigmaS(0.0f)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene)
    {
        _scene = &scene;

        Medium::fromJson(v, scene);
        JsonUtils::fromJson(v, "pos", _pos);
        JsonUtils::fromJson(v, "scale", _scale);
        JsonUtils::fromJson(v, "pivot", _primName);
        JsonUtils::fromJson(v, "cloud_density", _cloudDensity);
        JsonUtils::fromJson(v, "cloud_albedo", _cloudAlbedo);
        JsonUtils::fromJson(v, "cloud_shift", _cloudShift);
        JsonUtils::fromJson(v, "background_sigma_s", _backgroundSigmaS);

        const rapidjson::Value::Member *cloudThickness = v.FindMember("cloud_thickness");
        if (cloudThickness) {
            _cloudThickness = scene.fetchTexture(cloudThickness->value, TexelConversion::REQUEST_AVERAGE);

            if (!JsonUtils::fromJson(v, "cloud_min_radius", _cloudMinR) ||
                !JsonUtils::fromJson(v, "cloud_max_radius", _cloudMaxR)) {
                _cloudThickness = nullptr;
            }
        }

        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(Medium::toJson(allocator));
        v.AddMember("type", "atmosphere", allocator);
        v.AddMember("scale", _scale, allocator);
        v.AddMember("pos", JsonUtils::toJsonValue(_pos, allocator), allocator);
        if (!_primName.empty())
            v.AddMember("pivot", _primName.c_str(), allocator);
        if (_cloudThickness) {
            JsonUtils::addObjectMember(v, "cloud_thickness", *_cloudThickness, allocator);
            v.AddMember("cloud_min_radius", _cloudMinR, allocator);
            v.AddMember("cloud_max_radius", _cloudMaxR, allocator);
            v.AddMember("cloud_density", _cloudDensity, allocator);
            v.AddMember("cloud_albedo", _cloudAlbedo, allocator);
            v.AddMember("cloud_shift", _cloudShift, allocator);
            v.AddMember("background_sigma_s", JsonUtils::toJsonValue(_backgroundSigmaS, allocator), allocator);
        }

        return std::move(v);
    }

    virtual bool isHomogeneous() const override final
    {
        return false;
    }

    virtual void prepareForRender() override final
    {
        if (!_primName.empty()) {
            const Primitive *prim = _scene->findPrimitive(_primName);
            if (!prim) {
                DBG("Note: unable to find pivot object '%s' for atmospheric medium", _primName.c_str());
                return;
            }
            _pos = prim->transform()*Vec3f(0.0f);
            _scale = Rg/(prim->transform().extractScale()*Vec3f(1.0f)).max();
        }

        _sigmaS = 1.0f/FalloffScale*Vec3f(
            float(double(BetaR)*double(_scale)),
            float(double(BetaG)*double(_scale)),
            float(double(BetaB)*double(_scale))
        );

        _rG = float(double(Rg)/double(_scale));
        _rT = float(double((Rt - Rg)*FalloffScale + Rg)/double(_scale));
        _hR = float(FalloffScale*double(Hr)/double(_scale));
    }

    virtual void cleanupAfterRender() override final
    {
    }

    virtual bool sampleDistance(VolumeScatterEvent &event, MediumState &state) const override final
    {
        if (state.bounce > _maxBounce)
            return false;

        Vec2f tSpan;
        if (!sphereMinTMaxT(event.p, event.wi, tSpan, _rT) || event.maxT < tSpan.x()) {
            event.t = event.maxT;
            event.throughput = Vec3f(1.0f);
            return true;
        }

        if (state.firstScatter) {
            float cdf0 = event.currentThroughput.x();
            float cdf1 = event.currentThroughput.y() + cdf0;
            float cdf2 = event.currentThroughput.z() + cdf1;
            cdf0 /= cdf2;
            cdf1 /= cdf2;

            float pick = event.supplementalSampler->next1D();
            float componentWeight;
            if (pick < cdf0) {
                state.component = 0;
                componentWeight = 1.0f/cdf0;
            } else if (pick < cdf1) {
                state.component = 1;
                componentWeight = 1.0f/(cdf1 - cdf0);
            } else {
                state.component = 2;
                componentWeight = 1.0f/(1.0f - cdf1);
            }
            event.throughput = Vec3f(0.0f);
            event.throughput[state.component] = componentWeight;
        } else {
            event.throughput = Vec3f(0.0f);
            event.throughput[state.component] = 1.0f;
        }
        state.advance();

        float targetDepth = -std::log(1.0f - event.sampler->next1D())/_sigmaS[state.component];
        float minT = max(0.0f, tSpan.x());
        Vec3f p = event.p + minT*event.wi;
        float maxT = min(event.maxT - minT, tSpan.y());

        float sampledT;
        if (_cloudThickness) {
            targetDepth *= _sigmaS[state.component];
            Vec4f depthAndT = spectralOpticalDepthAndT(p, event.wi, maxT, targetDepth, state.component, event.sampler->next1D());
            sampledT = depthAndT.w();
        } else {
            Vec2f depthAndT = opticalDepthAndT(p, event.wi, maxT, targetDepth);
            sampledT = depthAndT.y();
        }

        event.t = sampledT == maxT ? event.maxT : (sampledT + minT);

        return true;
    }

    virtual bool absorb(VolumeScatterEvent &event, MediumState &state) const override final
    {
        if (_cloudThickness) {
            Vec3f localP = event.p - _pos;
            float r = localP.length();
            bool insideClouds = (r - _rG >= _cloudMinR && r - _rG <= _cloudMaxR);
            if (insideClouds && useCloudDensity(localP/r, r)) {
                if (event.sampler->next1D() >= _cloudAlbedo)
                    return true;
            }
        }

        event.throughput = Vec3f(0.0f);
        event.throughput[state.component] = 1.0f;
        return false;
    }

    virtual bool scatter(VolumeScatterEvent &event) const override final
    {
        if (_cloudThickness) {
            Vec3f localP = event.p - _pos;
            float r = localP.length();
            bool insideClouds = (r - _rG >= _cloudMinR && r - _rG <= _cloudMaxR);
            if (insideClouds && useCloudDensity(localP/r, r)) {
                event.wo  = PhaseFunction::sample(PhaseFunction::Isotropic, 0.0f, event.sampler->next2D());
                event.pdf = PhaseFunction::eval(PhaseFunction::Isotropic, event.wo.z(), 0.0f);
                return true;
            }
        }
        event.wo = PhaseFunction::sample(PhaseFunction::Rayleigh, 0.0f, event.sampler->next2D());
        event.pdf = PhaseFunction::eval(PhaseFunction::Rayleigh, event.wo.z(), 0.0f);
        TangentFrame frame(event.wi);
        event.wo = frame.toGlobal(event.wo);
        return true;
    }

    Vec2f densityAndDerivative(float r, float mu, float t, float d) const
    {
        float density = std::exp((_rG - d)/_hR);
        float dDdT = std::abs((t + r*mu)/(d*_hR))*max(density, 0.01f);
        return Vec2f(density, dDdT);
    }

    virtual Vec3f transmittance(const VolumeScatterEvent &event) const override final
    {
        Vec2f tSpan;
        if (!sphereMinTMaxT(event.p, event.wi, tSpan, _rT) || event.maxT < tSpan.x())
            return Vec3f(1.0f);

        float minT = max(0.0f, tSpan.x());
        Vec3f p = event.p + minT*event.wi;
        float maxT = min(event.maxT - minT, tSpan.y());

        if (_cloudThickness) {
            Vec4f depthAndT = spectralOpticalDepthAndT(p, event.wi, maxT, 1.0f, 0, 1.0f);

            return std::exp(-depthAndT.xyz());
        } else {
            Vec2f depthAndT = opticalDepthAndT(p, event.wi, maxT, 1.0f);

            return std::exp(-_sigmaS*depthAndT.x());
        }
    }

    virtual Vec3f emission(const VolumeScatterEvent &/*event*/) const override final
    {
        return Vec3f(0.0f);
    }

    virtual Vec3f eval(const VolumeScatterEvent &event) const override final
    {
        if (_cloudThickness) {
            Vec3f localP = event.p - _pos;
            float r = localP.length();
            bool insideClouds = (r - _rG >= _cloudMinR && r - _rG <= _cloudMaxR);
            if (insideClouds && useCloudDensity(localP/r, r))
                return Vec3f(_cloudAlbedo*PhaseFunction::eval(PhaseFunction::Isotropic, event.wi.dot(event.wo), 0.0f));
        }

        return Vec3f(PhaseFunction::eval(PhaseFunction::Rayleigh, event.wi.dot(event.wo), 0.0f));
    }
};

}

#endif /* ATMOSPHERICMEDIUM_HPP_ */
