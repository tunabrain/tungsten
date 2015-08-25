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

void AtmosphericMedium::sampleColorChannel(PathSampleGenerator &sampler, MediumState &state, MediumSample &sample) const
{
    state.component = sampler.nextDiscrete(DiscreteTransmittanceSample, 3);
    sample.pdf = 1.0f/3.0f;
    sample.weight = Vec3f(0.0f);
    sample.weight[state.component] = 3.0f;
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

bool AtmosphericMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    if (state.bounce > _maxBounce)
        return false;

    sample.phase = _phaseFunction.get();

    Vec2f tSpan;
    if (!sphereMinTMaxT(ray.pos(), ray.dir(), tSpan, _rT) || ray.farT() < tSpan.x()) {
        sample.t = ray.farT();
        sample.weight = Vec3f(1.0f);
        sample.pdf = 1.0f;
        sample.p = ray.pos() + sample.t*ray.dir();
        sample.exited = true;
        return true;
    }

    if (state.firstScatter) {
        sampleColorChannel(sampler, state, sample);
    } else {
        sample.pdf = 1.0f;
        sample.weight = Vec3f(0.0f);
        sample.weight[state.component] = 1.0f;
    }
    state.advance();

    float sigmaSc = _sigmaS[state.component];
    float targetDepth = -std::log(1.0f - sampler.next1D(MediumTransmittanceSample))/sigmaSc;
    float minT = max(0.0f, tSpan.x());
    Vec3f p = ray.pos() + minT*ray.dir();
    float maxT = min(ray.farT(), tSpan.y()) - minT;

    Vec2f depthAndT = opticalDepthAndT(p, ray.dir(), maxT, targetDepth);
    float sampledT = depthAndT.y();
    sample.exited = sampledT == maxT;

    float extinction = std::exp(-sigmaSc*depthAndT.x());
    if (sample.exited) {
        sample.t = ray.farT();
        sample.pdf *= sigmaSc*extinction;
    } else {
        sample.t = (sampledT + minT);
        sample.pdf *= extinction;
    }
    sample.p = ray.pos() + sample.t*ray.dir();

    return true;
}

Vec3f AtmosphericMedium::transmittance(const Ray &ray) const
{
    Vec2f tSpan;
    if (!sphereMinTMaxT(ray.pos(), ray.dir(), tSpan, _rT) || ray.farT() < tSpan.x())
        return Vec3f(1.0f);

    float minT = max(0.0f, tSpan.x());
    Vec3f p = ray.pos() + minT*ray.dir();
    float maxT = min(ray.farT() - minT, tSpan.y());

    Vec2f depthAndT = opticalDepthAndT(p, ray.dir(), maxT, 1.0f);

    return std::exp(-_sigmaS*depthAndT.x());
}

float AtmosphericMedium::pdf(const Ray &/*ray*/, bool /*onSurface*/) const
{
    return 0.0f; // TODO: Broken, for now. Figure this out later
}

}
