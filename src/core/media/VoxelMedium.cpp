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

void VoxelMedium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Medium::fromJson(v, scene);
    JsonUtils::fromJson(v, "sigma_a", _sigmaA);
    JsonUtils::fromJson(v, "sigma_s", _sigmaS);
    _grid = scene.fetchGrid(JsonUtils::fetchMember(v, "grid"));
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
        sample.weight = _grid->transmittance(sampler, p, w, t0, t1, _sigmaT/wPrime);
        sample.pdf = 1.0f;
        sample.exited = true;
    } else {
        int component = sampler.nextDiscrete(3);
        float sigmaTc = _sigmaT[component];
        float xi = 1.0f - sampler.next1D();
        float logXi = -std::log(xi);

        Vec2f tAndDensity = _grid->inverseOpticalDepth(sampler, p, w, t0, t1, sigmaTc/wPrime, logXi);
        sample.t = tAndDensity.x();
        sample.exited = (sample.t >= t1);
        float opticalDepth = sample.exited ? tAndDensity.y()/sigmaTc : logXi/sigmaTc;
        sample.weight = std::exp(-_sigmaT*opticalDepth);
        if (sample.exited) {
            sample.pdf = sample.weight.avg();
        } else {
            float rho = tAndDensity.y();
            sample.pdf = (rho*_sigmaT*sample.weight).avg();
            sample.weight *= rho*_sigmaS;
        }
        sample.weight /= sample.pdf;
        sample.t /= wPrime;

        state.advance();
    }
    sample.p = ray.pos() + sample.t*ray.dir();
    sample.phase = _phaseFunction.get();

    return true;
}

Vec3f VoxelMedium::transmittance(PathSampleGenerator &sampler, const Ray &ray) const
{
    Vec3f p = _worldToGrid*ray.pos();
    Vec3f w = _worldToGrid.transformVector(ray.dir());
    float wPrime = w.length();
    w /= wPrime;
    float t0 = 0.0f, t1 = ray.farT()*wPrime;
    if (!bboxIntersection(_gridBounds, p, w, t0, t1))
        return Vec3f(1.0f);

    return _grid->transmittance(sampler, p, w, t0, t1, _sigmaT/wPrime);
}

float VoxelMedium::pdf(PathSampleGenerator &sampler, const Ray &ray, bool onSurface) const
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

        Vec3f transmittance = _grid->transmittance(sampler, p, w, t0, t1, _sigmaT/wPrime);
        if (onSurface) {
            return transmittance.avg();
        } else {
            return (_grid->density(p)*_sigmaT*transmittance).avg();
        }
    }
}

Vec3f VoxelMedium::transmittanceAndPdfs(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
        bool endOnSurface, float &pdfForward, float &pdfBackward) const
{
    Vec3f p = _worldToGrid*ray.pos();
    Vec3f w = _worldToGrid.transformVector(ray.dir());
    float wPrime = w.length();
    w /= wPrime;
    float t0 = 0.0f, t1 = ray.farT()*wPrime;
    if (!bboxIntersection(_gridBounds, p, w, t0, t1)) {
        pdfForward = pdfBackward = 1.0f;
        return Vec3f(1.0f);
    }

    Vec3f transmittance = _grid->transmittance(sampler, p, w, t0, t1, _sigmaT/wPrime);

    if (_absorptionOnly) {
        pdfForward = pdfBackward = 1.0f;
    } else {
        pdfForward  =   endOnSurface ? transmittance.avg() : (_grid->density(p)*_sigmaT*transmittance).avg();
        pdfBackward = startOnSurface ? transmittance.avg() : (_grid->density(p)*_sigmaT*transmittance).avg();
    }

    return transmittance;
}

}
