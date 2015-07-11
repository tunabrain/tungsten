#include "AtmosphericMedium.hpp"

#include "primitives/Primitive.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

AtmosphericMedium::AtmosphericMedium()
: _scene(nullptr),
  _pos(0.0f),
  _scale(Rg)
{
}

bool AtmosphericMedium::sphereMinTMaxT(const Vec3f &o, const Vec3f &w, Vec2f &tSpan, float r) const
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

Vec2f AtmosphericMedium::densityAndDerivative(float r, float mu, float t, float d) const
{
    float density = std::exp((_rG - d)/_hR);
    float dDdT = std::abs((t + r*mu)/(d*_hR))*max(density, 0.01f);
    return Vec2f(density, dDdT);
}

void AtmosphericMedium::sampleColorChannel(VolumeScatterEvent &event, MediumState &state) const
{
    state.component = event.sampler->nextDiscrete(DiscreteTransmittanceSample, 3);
    event.weight = Vec3f(0.0f);
    event.weight[state.component] = 1.0f/3.0f;
}

Vec2f AtmosphericMedium::opticalDepthAndT(const Vec3f &p, const Vec3f &w, float maxT, float targetDepth) const
{
    CONSTEXPR int MaxStepCount = 1024*10;
    CONSTEXPR float maxError = 0.02f;
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

void AtmosphericMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    _scene = &scene;

    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "pos", _pos);
    JsonUtils::fromJson(v, "scale", _scale);
    JsonUtils::fromJson(v, "pivot", _primName);

    init();
}

rapidjson::Value AtmosphericMedium::toJson(Allocator &allocator) const
{
    rapidjson::Value v(Medium::toJson(allocator));
    v.AddMember("type", "atmosphere", allocator);
    v.AddMember("scale", _scale, allocator);
    v.AddMember("pos", JsonUtils::toJsonValue(_pos, allocator), allocator);
    if (!_primName.empty())
        v.AddMember("pivot", _primName.c_str(), allocator);

    return std::move(v);
}

bool AtmosphericMedium::isHomogeneous() const
{
    return false;
}

void AtmosphericMedium::prepareForRender()
{
    if (!_primName.empty()) {
        const Primitive *prim = _scene->findPrimitive(_primName);
        if (!prim) {
            DBG("Note: unable to find pivot object '%s' for atmospheric medium", _primName.c_str());
        } else {
            _pos = prim->transform()*Vec3f(0.0f);
            _scale = Rg/(prim->transform().extractScale()*Vec3f(1.0f)).max();
        }
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

void AtmosphericMedium::teardownAfterRender()
{
}

bool AtmosphericMedium::sampleDistance(VolumeScatterEvent &event, MediumState &state) const
{
    if (state.bounce > _maxBounce)
        return false;

    Vec2f tSpan;
    if (!sphereMinTMaxT(event.p, event.wi, tSpan, _rT) || event.maxT < tSpan.x()) {
        event.t = event.maxT;
        event.weight = Vec3f(1.0f);
        return true;
    }

    if (state.firstScatter) {
        sampleColorChannel(event, state);
    } else {
        event.weight = Vec3f(0.0f);
        event.weight[state.component] = 1.0f;
    }
    state.advance();

    float targetDepth = -std::log(1.0f - event.sampler->next1D(TransmittanceSample))/_sigmaS[state.component];
    float minT = max(0.0f, tSpan.x());
    Vec3f p = event.p + minT*event.wi;
    float maxT = min(event.maxT - minT, tSpan.y());

    Vec2f depthAndT = opticalDepthAndT(p, event.wi, maxT, targetDepth);
    float sampledT = depthAndT.y();

    event.t = sampledT == maxT ? event.maxT : (sampledT + minT);

    return true;
}

bool AtmosphericMedium::absorb(VolumeScatterEvent &event, MediumState &state) const
{
    event.weight = Vec3f(0.0f);
    event.weight[state.component] = 1.0f;
    return false;
}

bool AtmosphericMedium::scatter(VolumeScatterEvent &event) const
{
    event.wo = PhaseFunction::sample(PhaseFunction::Rayleigh, 0.0f, event.sampler->next2D(MediumSample));
    event.pdf = PhaseFunction::eval(PhaseFunction::Rayleigh, event.wo.z(), 0.0f);
    TangentFrame frame(event.wi);
    event.wo = frame.toGlobal(event.wo);
    return true;
}

Vec3f AtmosphericMedium::transmittance(const VolumeScatterEvent &event) const
{
    Vec2f tSpan;
    if (!sphereMinTMaxT(event.p, event.wi, tSpan, _rT) || event.maxT < tSpan.x())
        return Vec3f(1.0f);

    float minT = max(0.0f, tSpan.x());
    Vec3f p = event.p + minT*event.wi;
    float maxT = min(event.maxT - minT, tSpan.y());

    Vec2f depthAndT = opticalDepthAndT(p, event.wi, maxT, 1.0f);

    return std::exp(-_sigmaS*depthAndT.x());
}

Vec3f AtmosphericMedium::emission(const VolumeScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

Vec3f AtmosphericMedium::phaseEval(const VolumeScatterEvent &event) const
{
    return Vec3f(PhaseFunction::eval(PhaseFunction::Rayleigh, event.wi.dot(event.wo), 0.0f));
}

}
