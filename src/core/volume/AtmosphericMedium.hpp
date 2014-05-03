#ifndef ATMOSPHERICMEDIUM_HPP_
#define ATMOSPHERICMEDIUM_HPP_

#include "Medium.hpp"

#include "primitives/Primitive.hpp"

namespace Tungsten
{

class AtmosphericMedium : public Medium
{
    static constexpr float Rg = 6360.0f*1e3f;
    static constexpr float Rt = 6420.0f*1e3f;
    static constexpr float Hr = 8.0f*1e3f;
    static constexpr float BetaR = 5.8f*1e-6f;
    static constexpr float BetaG = 13.5f*1e-6f;
    static constexpr float BetaB = 33.1f*1e-6f;
    static constexpr float FalloffScale = 50.0f;

    const Scene *_scene;

    std::string _primName;

    Vec3f _pos;
    float _scale;

    Vec3f _sigmaS;
    float _rG;
    float _rT;
    float _hR;

    bool atmosphereMinTMaxT(const Vec3f &o, const Vec3f &w, Vec2f &tSpan) const
    {
        Vec3f p = o - _pos;
        float B = p.dot(w);
        float C = p.lengthSq() - _rT*_rT;
        float detSq = B*B - C;
        if (detSq >= 0.0f) {
            float det = std::sqrt(detSq);
            tSpan.x() = -B - det;
            tSpan.y() = -B + det;
            return true;
        }

        return false;
    }

    Vec2f opticalDepthAndT(const Vec3f &p, const Vec3f &w, float maxT, float targetDepth) const
    {
        constexpr float maxError = 0.02f;

        Vec3f localP = p - _pos;
        float r = localP.length();
        float mu = w.dot(localP)/r;

        float opticalDepth = 0.0f;
        Vec2f dD = densityAndDerivative(r, mu, 0.0f);
        float dT = min(maxError/dD.y(), maxT*0.2f);
        float t = dT;
        while (t < maxT) {
            Vec2f newDd = densityAndDerivative(r, mu, t);
            float depthStep = (dD.x() + newDd.x())*0.5f*dT;
            opticalDepth += depthStep;
            if (opticalDepth >= targetDepth) {
                float alpha = (targetDepth - opticalDepth + depthStep)/depthStep;
                t -= dT*(1.0f - alpha);
                return Vec2f(targetDepth, t);
            }

            dD = newDd;
            dT = min(maxError/dD.y(), maxT*0.2f);
            t += dT;
        }
        Vec2f newDd = densityAndDerivative(r, mu, maxT);
        opticalDepth += (dD.x() + newDd.x())*0.5f*(maxT - t + dT);

        return Vec2f(opticalDepth, maxT);
    }

public:
    AtmosphericMedium()
    : _scene(nullptr), _pos(0.0f), _scale(Rg)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene)
    {
        _scene = &scene;

        Medium::fromJson(v, scene);
        JsonUtils::fromJson(v, "pos", _pos);
        JsonUtils::fromJson(v, "scale", _scale);
        JsonUtils::fromJson(v, "pivot", _primName);
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

        std::cout << _rG << " " << _hR << " " << _sigmaS << std::endl;
    }

    virtual void cleanupAfterRender() override final
    {
    }

    virtual bool sampleDistance(VolumeScatterEvent &event, MediumState &state) const override final
    {
        if (state.bounce > _maxBounce)
            return false;

        Vec2f tSpan;
        if (!atmosphereMinTMaxT(event.p, event.wi, tSpan) || event.maxT < tSpan.x()) {
            event.t = event.maxT;
            event.throughput = Vec3f(1.0f);
            return true;
        }

        if (state.firstScatter)
            state.component = event.supplementalSampler->nextI() % 3;
        state.advance();

        float targetDepth = -std::log(1.0f - event.sampler->next1D())/_sigmaS[state.component];
        float minT = max(0.0f, tSpan.x());
        Vec3f p = event.p + minT*event.wi;
        float maxT = min(event.maxT - minT, tSpan.y());

        Vec2f depthAndT = opticalDepthAndT(p, event.wi, maxT, targetDepth);

        event.t = depthAndT.y() == maxT ? event.maxT : (depthAndT.y() + minT);
        event.throughput = Vec3f(0.0f);
        event.throughput[state.component] = 3.0f;

        return true;
    }

    virtual bool absorb(VolumeScatterEvent &event, MediumState &state) const override final
    {
        event.throughput = Vec3f(0.0f);
        event.throughput[state.component] = 1.0f;
        return false;
    }

    virtual bool scatter(VolumeScatterEvent &event) const override final
    {
        event.wo = PhaseFunction::sample(PhaseFunction::Rayleigh, 0.0f, event.sampler->next2D());
        event.pdf = PhaseFunction::eval(PhaseFunction::Rayleigh, event.wo.z(), 0.0f);
        TangentFrame frame(event.wi);
        event.wo = frame.toGlobal(event.wo);
        return true;
    }

    Vec2f densityAndDerivative(float r, float mu, float t) const
    {
        float d = std::sqrt(r*r + t*t + 2.0f*r*t*mu);
        float density = std::exp((_rG - d)/_hR);
        float dDdT = std::abs((t + r*mu)/(d*_hR))*max(density, 0.01f);
        return Vec2f(density, dDdT);
    }

    virtual Vec3f transmittance(const VolumeScatterEvent &event) const override final
    {
        Vec2f depthAndT = opticalDepthAndT(event.p, event.wi, event.maxT, 1.0f);

        return std::exp(-_sigmaS*depthAndT.x());
    }

    virtual Vec3f emission(const VolumeScatterEvent &/*event*/) const override final
    {
        return Vec3f(0.0f);
    }

    virtual Vec3f eval(const VolumeScatterEvent &event) const override final
    {
        return Vec3f(PhaseFunction::eval(PhaseFunction::Rayleigh, event.wi.dot(event.wo), 0.0f));
    }
};

}

#endif /* ATMOSPHERICMEDIUM_HPP_ */
