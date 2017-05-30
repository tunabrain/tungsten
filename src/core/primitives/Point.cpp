#include "Point.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include "Debug.hpp"

namespace Tungsten {

Point::Point(Mat4f &transform)
{
    _transform = transform;
}

void Point::buildProxy()
{
    _proxy = std::make_shared<TriangleMesh>();
    _proxy->makeSphere(0.05f);
}

float Point::powerToRadianceFactor() const
{
    return INV_FOUR_PI;
}

void Point::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);
}

rapidjson::Value Point::toJson(Allocator &allocator) const
{
    return JsonObject{Primitive::toJson(allocator), allocator,
        "type", "point"
    };
}

bool Point::intersect(Ray &/*ray*/, IntersectionTemporary &/*data*/) const
{
    return false;
}

bool Point::occluded(const Ray &/*ray*/) const
{
    return false;
}

bool Point::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void Point::intersectionInfo(const IntersectionTemporary &/*data*/, IntersectionInfo &info) const
{
    info.Ng = info.Ns = -info.w;
    info.uv = Vec2f(0.0f);
}

bool Point::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool Point::isSamplable() const
{
    return true;
}

void Point::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Point::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &sample) const
{
    sample.p = _pos;
    sample.pdf = 1.0f;
    sample.uv = Vec2f(0.0f);
    sample.weight = FOUR_PI*(*_emission)[Vec2f(0.0f)];
    sample.Ng = Vec3f(0.0f);

    return true;
}

bool Point::sampleDirection(PathSampleGenerator &sampler, const PositionSample &/*point*/, DirectionSample &sample) const
{
    sample.d = SampleWarp::uniformSphere(sampler.next2D());
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::uniformSpherePdf();

    return true;
}

bool Point::sampleDirect(uint32 /*threadIndex*/, const Vec3f &p, PathSampleGenerator &/*sampler*/, LightSample &sample) const
{
    sample.d = _pos - p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.pdf = rSq;
    return true;
}

bool Point::invertPosition(WritablePathSampleGenerator &/*sampler*/, const PositionSample &/*point*/) const
{
    return true;
}

bool Point::invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/,
        const DirectionSample &direction) const
{
    sampler.put2D(SampleWarp::invertUniformSphere(direction.d, sampler.untracked1D()));
    return true;
}

float Point::positionalPdf(const PositionSample &/*point*/) const
{
    return 1.0f;
}

float Point::directionalPdf(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return SampleWarp::uniformSpherePdf();
}

float Point::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &p) const
{
    return (p - _pos).lengthSq();
}

Vec3f Point::evalPositionalEmission(const PositionSample &/*sample*/) const
{
    return FOUR_PI*(*_emission)[Vec2f(0.0f)];
}

Vec3f Point::evalDirectionalEmission(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return Vec3f(INV_FOUR_PI);
}

Vec3f Point::evalDirect(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/) const
{
    return (*_emission)[Vec2f(0.0f)];
}

bool Point::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Point::isDirac() const
{
    return true;
}

bool Point::isInfinite() const
{
    return false;
}

float Point::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &p) const
{
    return INV_FOUR_PI*_power.max()/(_pos - p).lengthSq();
}

Box3f Point::bounds() const
{
    return Box3f(_pos);
}

const TriangleMesh &Point::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Point::prepareForRender()
{
    _pos = _transform.extractTranslationVec();
    _power = _emission ? FOUR_PI*_emission->average() : Vec3f(0.0f);

    Primitive::prepareForRender();
}

int Point::numBsdfs() const
{
    return 0;
}

std::shared_ptr<Bsdf> &Point::bsdf(int /*index*/)
{
    FAIL("Point::bsdf should never be called");
}

void Point::setBsdf(int /*index*/, std::shared_ptr<Bsdf> &/*bsdf*/)
{
    FAIL("Point::setBsdf should never be called");
}

Primitive *Point::clone()
{
    return new Point(_transform);
}

}
