#include "VoxelMedium.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "math/TangentFrame.hpp"
#include "math/Ray.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

VoxelMedium::VoxelMedium()
: _sigmaA(0.0f),
  _sigmaS(0.0f)
{
}

void VoxelMedium::fromJson(JsonPtr value, const Scene &scene)
{
    Medium::fromJson(value, scene);
    value.getField("sigma_a", _sigmaA);
    value.getField("sigma_s", _sigmaS);
    _grid = scene.fetchGrid(value.getRequiredMember("grid"));
}

rapidjson::Value VoxelMedium::toJson(Allocator &allocator) const
{
    return JsonObject{Medium::toJson(allocator), allocator,
        "type", "voxel",
        "sigma_a", _sigmaA,
        "sigma_s", _sigmaS,
        "grid", *_grid
    };
}

void VoxelMedium::loadResources()
{
    _grid->loadResources();
}

bool VoxelMedium::isHomogeneous() const
{
    return true;
}

void VoxelMedium::prepareForRender()
{
    _sigmaT = _sigmaA + _sigmaS;
    _absorptionOnly = _sigmaS == 0.0f;

    _worldToGrid = _grid->invNaturalTransform();
    _gridBounds = _grid->bounds();

    std::cout << _worldToGrid << std::endl;
    std::cout << _gridBounds << std::endl;
    std::cout << Box3f(_grid->naturalTransform()*_gridBounds.min(),
                       _grid->naturalTransform()*_gridBounds.max()) << std::endl;
}

static inline bool bboxIntersection(const Box3f &box, const Vec3f &o, const Vec3f &d,
        float &tMin, float &tMax)
{
    Vec3f invD = 1.0f/d;
    Vec3f relMin((box.min() - o));
    Vec3f relMax((box.max() - o));

    float ttMin = tMin, ttMax = tMax;
    for (int i = 0; i < 3; ++i) {
        if (invD[i] >= 0.0f) {
            ttMin = max(ttMin, relMin[i]*invD[i]);
            ttMax = min(ttMax, relMax[i]*invD[i]);
        } else {
            ttMax = min(ttMax, relMin[i]*invD[i]);
            ttMin = max(ttMin, relMax[i]*invD[i]);
        }
    }

    if (ttMin <= ttMax) {
        tMin = ttMin;
        tMax = ttMax;
        return true;
    }
    return false;
}

Vec3f VoxelMedium::sigmaA(Vec3f p) const
{
    return _grid->density(p)*_sigmaA;
}

Vec3f VoxelMedium::sigmaS(Vec3f p) const
{
    return _grid->density(p)*_sigmaS;
}

Vec3f VoxelMedium::sigmaT(Vec3f p) const
{
    return _grid->density(p)*_sigmaT;
}

bool VoxelMedium::sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
        MediumState &state, MediumSample &sample) const
{
    if (state.bounce > _maxBounce)
        return false;

    float maxT = ray.farT();
    Vec3f p = _worldToGrid*ray.pos();
    Vec3f w = _worldToGrid.transformVector(ray.dir());
    float wPrime = w.length();
    w /= wPrime;
    float t0 = 0.0f, t1 = maxT*wPrime;
    if (!bboxIntersection(_gridBounds, p, w, t0, t1)) {
        sample.t = maxT;
        sample.weight = Vec3f(1.0f);
        sample.pdf = 1.0f;
        sample.exited = true;
        return true;
    }

    if (_absorptionOnly) {
        sample.t = maxT;
        Vec3f tau = _grid->opticalDepth(sampler, p, w, t0, t1)*(_sigmaT/wPrime);
        sample.weight = _transmittance->eval(tau, state.firstScatter, true);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float tauC = _transmittance->sample(sampler, state.firstScatter)/(sigmaTc/wPrime);

        Vec2f tAndDensity = _grid->inverseOpticalDepth(sampler, p, w, t0, t1, tauC);
        sample.t = tAndDensity.x();
        sample.exited = (sample.t >= t1);
        if (sample.exited)
            tauC = tAndDensity.y();
        Vec3f tau = tauC*(_sigmaT/wPrime);
        sample.weight = _transmittance->eval(tau, state.firstScatter, sample.exited);
        if (sample.exited) {
            sample.pdf = _transmittance->surfaceProbability(tau, state.firstScatter).avg();
        } else {
            float rho = tAndDensity.y();
            sample.pdf = (rho*_sigmaT*_transmittance->mediumPdf(tau, state.firstScatter)).avg();
            sample.weight *= rho*_sigmaS*_transmittance->sigmaBar();
        }
        sample.weight /= sample.pdf;
        sample.t /= wPrime;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}

Vec3f VoxelMedium::transmittance(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
        bool endOnSurface) const
{
    Vec3f p = _worldToGrid*ray.pos();
    Vec3f w = _worldToGrid.transformVector(ray.dir());
    float wPrime = w.length();
    w /= wPrime;
    float t0 = 0.0f, t1 = ray.farT()*wPrime;
    if (!bboxIntersection(_gridBounds, p, w, t0, t1))
        return Vec3f(1.0f);

    Vec3f tau = _grid->opticalDepth(sampler, p, w, t0, t1)*(_sigmaT/wPrime);
    return _transmittance->eval(tau, startOnSurface, endOnSurface);
}

float VoxelMedium::pdf(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface, bool endOnSurface) const
{
    if (_absorptionOnly) {
        return 1.0f;
    } else {
        Vec3f p = _worldToGrid*ray.pos();
        Vec3f w = _worldToGrid.transformVector(ray.dir());
        float wPrime = w.length();
        w /= wPrime;
        float t0 = 0.0f, t1 = ray.farT()*wPrime;
        if (!bboxIntersection(_gridBounds, p, w, t0, t1))
            return 1.0f;

        Vec3f tau = _grid->opticalDepth(sampler, p, w, t0, t1)*(_sigmaT/wPrime);
        if (endOnSurface)
            return _transmittance->surfaceProbability(tau, startOnSurface).avg();
        else
            return (_grid->density(p)*_sigmaT*_transmittance->mediumPdf(tau, startOnSurface)).avg();
    }
}

}
