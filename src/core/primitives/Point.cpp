#include "Point.hpp"
#include "TriangleMesh.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

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

void Point::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);
}

rapidjson::Value Point::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "point", allocator);
    return std::move(v);
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

void Point::intersectionInfo(const IntersectionTemporary &/*data*/, IntersectionInfo &/*info*/) const
{
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

void Point::makeSamplable(uint32 /*threadIndex*/)
{
}

float Point::inboundPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
{
    return 0.0f;
}

bool Point::sampleInboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    sample.d = _pos - sample.p;
    float rSq = sample.d.lengthSq();
    sample.dist = std::sqrt(rSq);
    sample.d /= sample.dist;
    sample.pdf = rSq;
    return true;
}

bool Point::sampleOutboundDirection(uint32 /*threadIndex*/, LightSample &sample) const
{
    Vec2f xi = sample.sampler->next2D();
    sample.p = _pos;
    sample.d = SampleWarp::uniformSphere(xi);
    sample.pdf = SampleWarp::uniformSpherePdf();
    sample.weight = _power;
    sample.medium = nullptr;
    return true;
}

bool Point::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Point::isDelta() const
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
}

void Point::teardownAfterRender()
{
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
