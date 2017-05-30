#include "Skydome.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/NullBsdf.hpp"

#include "math/Spectral.hpp"
#include "math/Angle.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include <skylight/ArHosekSkyModel.h>
#include <iostream>
#include <cstring>

namespace Tungsten {

struct SkydomeIntersection
{
    Vec3f p;
    Vec3f w;
};

Skydome::Skydome()
: _scene(nullptr),
  _temperature(5777.0f),
  _gammaScale(1.0f),
  _turbidity(3.0f),
  _intensity(2.0f),
  _doSample(true)
{
}

Vec2f Skydome::directionToUV(const Vec3f &wi) const
{
    return Vec2f(std::atan2(wi.z(), wi.x())*INV_TWO_PI + 0.5f, std::acos(-wi.y())*INV_PI);
}

Vec2f Skydome::directionToUV(const Vec3f &wi, float &sinTheta) const
{
    sinTheta = std::sqrt(max(1.0f - wi.y()*wi.y(), 0.0f));
    return Vec2f(std::atan2(wi.z(), wi.x())*INV_TWO_PI + 0.5f, std::acos(-wi.y())*INV_PI);
}

Vec3f Skydome::uvToDirection(Vec2f uv, float &sinTheta) const
{
    float phi   = (uv.x() - 0.5f)*TWO_PI;
    float theta = uv.y()*PI;
    sinTheta = std::sin(theta);
    return Vec3f(
        std::cos(phi)*sinTheta,
        -std::cos(theta),
        std::sin(phi)*sinTheta
    );
}

void Skydome::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(),
            std::make_shared<NullBsdf>(), "Sphere", false, false);
    _proxy->makeCone(0.05f, 1.0f);
}

float Skydome::powerToRadianceFactor() const
{
    return INV_FOUR_PI;
}

void Skydome::fromJson(JsonPtr value, const Scene &scene)
{
    _scene = &scene;

    Primitive::fromJson(value, scene);
    value.getField("temperature", _temperature);
    value.getField("gamma_scale", _gammaScale);
    value.getField("turbidity", _turbidity);
    value.getField("intensity", _intensity);
    value.getField("sample", _doSample);
}
rapidjson::Value Skydome::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "skydome",
        "temperature", _temperature,
        "gamma_scale", _gammaScale,
        "turbidity", _turbidity,
        "intensity", _intensity,
        "sample", _doSample
    };
}

bool Skydome::intersect(Ray &ray, IntersectionTemporary &data) const
{
    SkydomeIntersection *isect = data.as<SkydomeIntersection>();
    isect->p = ray.pos();
    isect->w = ray.dir();
    data.primitive = this;
    return true;
}

bool Skydome::occluded(const Ray &/*ray*/) const
{
    return true;
}

bool Skydome::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void Skydome::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const SkydomeIntersection *isect = data.as<SkydomeIntersection>();
    info.Ng = info.Ns = -isect->w;
    info.p = isect->p;
    info.uv = directionToUV(isect->w);
    info.primitive = this;
    info.bsdf = nullptr;
}

bool Skydome::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool Skydome::isSamplable() const
{
    return _doSample;
}

void Skydome::makeSamplable(const TraceableScene &scene, uint32 /*threadIndex*/)
{
    _sky->makeSamplable(MAP_SPHERICAL);
    _sceneBounds = scene.bounds();
    _sceneBounds.grow(1e-2f);
}

bool Skydome::samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const
{
    sample.uv = _sky->sample(MAP_SPHERICAL, sampler.next2D());
    float sinTheta;
    sample.Ng = -uvToDirection(sample.uv, sinTheta);

    float faceXi = sampler.next1D();
    Vec2f xi = sampler.next2D();
    sample.p = SampleWarp::projectedBox(_sceneBounds, sample.Ng, faceXi, xi);
    sample.pdf = SampleWarp::projectedBoxPdf(_sceneBounds, sample.Ng);
    sample.weight = Vec3f(1.0f/sample.pdf);

    return true;
}

bool Skydome::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &point, DirectionSample &sample) const
{
    sample.d = point.Ng;
    float sinTheta;
    directionToUV(-point.Ng, sinTheta);
    sample.pdf = INV_PI*INV_TWO_PI*_sky->pdf(MAP_SPHERICAL, point.uv)/sinTheta;
    if (sample.pdf == 0.0f)
        return false;
    sample.weight = (*_sky)[point.uv]/sample.pdf;

    return true;
}

bool Skydome::sampleDirect(uint32 /*threadIndex*/, const Vec3f &/*p*/, PathSampleGenerator &sampler, LightSample &sample) const
{
    Vec2f uv = _sky->sample(MAP_SPHERICAL, sampler.next2D());
    float sinTheta;
    sample.d = uvToDirection(uv, sinTheta);
    sample.pdf = INV_PI*INV_TWO_PI*_sky->pdf(MAP_SPHERICAL, uv)/sinTheta;
    sample.dist = Ray::infinity();
    return sample.pdf != 0.0f;
}

bool Skydome::invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const
{
    float faceXi;
    Vec2f xi;
    if (!SampleWarp::invertProjectedBox(_sceneBounds, point.p, -point.Ng, faceXi, xi, sampler.untracked1D()))
        return false;

    sampler.put1D(faceXi);
    sampler.put2D(xi);

    return true;
}

bool Skydome::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    sampler.put2D(_sky->invert(MAP_SPHERICAL, directionToUV(-direction.d)));

    return true;
}


float Skydome::positionalPdf(const PositionSample &point) const
{
    return SampleWarp::projectedBoxPdf(_sceneBounds, point.Ng);
}

float Skydome::directionalPdf(const PositionSample &point, const DirectionSample &/*sample*/) const
{
    float sinTheta;
    directionToUV(-point.Ng, sinTheta);
    return INV_PI*INV_TWO_PI*_sky->pdf(MAP_SPHERICAL, point.uv)/sinTheta;
}

float Skydome::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &data,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/) const
{
    const SkydomeIntersection *isect = data.as<SkydomeIntersection>();
    float sinTheta;
    Vec2f uv = directionToUV(isect->w, sinTheta);
    return INV_PI*INV_TWO_PI*_sky->pdf(MAP_SPHERICAL, uv)/sinTheta;
}

Vec3f Skydome::evalPositionalEmission(const PositionSample &/*sample*/) const
{
    return Vec3f(1.0f);
}

Vec3f Skydome::evalDirectionalEmission(const PositionSample &point, const DirectionSample &/*sample*/) const
{
    return (*_sky)[point.uv];
}

Vec3f Skydome::evalDirect(const IntersectionTemporary &/*data*/, const IntersectionInfo &info) const
{
    return (*_sky)[info.uv];
}

bool Skydome::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Skydome::isDirac() const
{
    return false;
}

bool Skydome::isInfinite() const
{
    return true;
}

float Skydome::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return FOUR_PI*_sky->average().max();
}

Box3f Skydome::bounds() const
{
    return Box3f(Vec3f(-1e30f), Vec3f(1e30f));
}

const TriangleMesh &Skydome::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

static CONSTEXPR int SizeX = 512;
static CONSTEXPR int SizeY = 256;
static CONSTEXPR int NumSamples = 10;

static void fillImage(ArHosekSkyModelState *state, float *lambdas, Vec3f *weights, Vec3f *img, Vec3f sun, float gammaScale)
{
    for (int y = 0; y < SizeY/2; ++y) {
        float theta = (y + 0.5f)*PI/SizeY;
        for (int x = 0; x < SizeX; ++x) {
            float phi = (x + 0.5f)*TWO_PI/SizeX;
            Vec3f v = Vec3f(std::cos(phi)*std::sin(theta), std::cos(theta), std::sin(phi)*std::sin(theta));
            float gamma = clamp(std::acos(clamp(v.dot(sun), -1.0f, 1.0f))*gammaScale, 0.0f, PI);

            Vec3f xyz(0.0f);
            for (int i = 0; i < NumSamples; ++i)
                xyz += weights[i]*arhosekskymodel_radiance(state, theta, gamma, lambdas[i]);

            img[x + y*SizeX] += Spectral::xyzToRgb(xyz);
        }
    }
}

void Skydome::prepareForRender()
{
    float lambdas[NumSamples];
    Vec3f weights[NumSamples];
    Spectral::spectralXyzWeights(NumSamples, lambdas, weights);

    Vec3f sun = _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f));

    float sunElevation = std::asin(clamp(sun.y(), -1.0f, 1.0f));

    ArHosekSkyModelState *sunState = arhosekskymodelstate_alienworld_alloc_init(sunElevation, _intensity,
            _temperature, _turbidity, 0.2f);

    std::unique_ptr<Vec3f[]> img(new Vec3f[SizeX*SizeY]);
    std::memset(img.get(), 0, SizeX*SizeY*sizeof(img[0]));

    fillImage(sunState, lambdas, weights, img.get(), sun, 1.0f);

    for (int y = SizeY/2; y < min(SizeY/2 + 2, SizeY); ++y)
        std::memcpy(img.get() + y*SizeX, img.get() + (SizeY/2 - 1)*SizeX, SizeX*sizeof(img[0]));

    _sky = std::make_shared<BitmapTexture>(img.release(), SizeX, SizeY, BitmapTexture::TexelType::RGB_HDR, true, false);
    _emission = _sky;

    Primitive::prepareForRender();
}

void Skydome::teardownAfterRender()
{
    _sky.reset();
    _emission.reset();

    Primitive::teardownAfterRender();
}

int Skydome::numBsdfs() const
{
    return 0;
}

std::shared_ptr<Bsdf> &Skydome::bsdf(int /*index*/)
{
    FAIL("Skydome::bsdf should not be called");
}

void Skydome::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &/*bsdf*/)
{
}

Primitive *Skydome::clone()
{
    return new Skydome(*this);
}

}
