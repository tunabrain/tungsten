#ifndef SKYDOME_HPP_
#define SKYDOME_HPP_

#include "Primitive.hpp"

#include "textures/BitmapTexture.hpp"

namespace Tungsten {

class Scene;

class Skydome : public Primitive
{
    const Scene *_scene;

    std::shared_ptr<BitmapTexture> _sky;
    float _temperature;
    float _gammaScale;
    float _turbidity;
    float _intensity;
    bool _doSample;

    std::shared_ptr<TriangleMesh> _proxy;

    Box3f _sceneBounds;

    Vec2f directionToUV(const Vec3f &wi) const;
    Vec2f directionToUV(const Vec3f &wi, float &sinTheta) const;
    Vec3f uvToDirection(Vec2f uv, float &sinTheta) const;
    void buildProxy();

protected:
    virtual float powerToRadianceFactor() const override;

public:
    Skydome();

    void setScene(const Scene *scene)
    {
        _scene = scene;
    }

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const override;
    virtual bool sampleDirect(uint32 threadIndex, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const override;
    virtual bool invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const;
    virtual bool invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &point,
            const DirectionSample &direction) const;
    virtual float positionalPdf(const PositionSample &point) const override;
    virtual float directionalPdf(const PositionSample &point, const DirectionSample &sample) const override;
    virtual float directPdf(uint32 threadIndex, const IntersectionTemporary &data,
            const IntersectionInfo &info, const Vec3f &p) const override;
    virtual Vec3f evalPositionalEmission(const PositionSample &sample) const override;
    virtual Vec3f evalDirectionalEmission(const PositionSample &point, const DirectionSample &sample) const override;
    virtual Vec3f evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDirac() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;
    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual void prepareForRender() override;
    virtual void teardownAfterRender() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;
    virtual void setBsdf(int index, std::shared_ptr<Bsdf> &bsdf) override;
    
    float turbidity() const
    {
        return _turbidity;
    }
    
    float intensity() const
    {
        return _intensity;
    }
    
    Vec3f sunDirection() const
    {
        return _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();
    }

    virtual Primitive *clone() override;
};

}

#endif /* SKYDOME_HPP_ */
