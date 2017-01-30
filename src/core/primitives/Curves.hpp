#ifndef CURVES_HPP_
#define CURVES_HPP_

#include "Primitive.hpp"

#include "bvh/BinaryBvh.hpp"

#include "io/Path.hpp"

#include "StringableEnum.hpp"

#include <memory>
#include <vector>

namespace Tungsten {

struct CurveIntersection;
class Scene;

class Curves : public Primitive
{
    enum CurveModeEnum
    {
        MODE_HALF_CYLINDER,
        MODE_BCSDF_CYLINDER,
        MODE_CYLINDER,
        MODE_RIBBON
    };
    typedef StringableEnum<CurveModeEnum> CurveMode;
    friend CurveMode;

    PathPtr _path;
    CurveMode _mode;
    float _curveThickness;
    float _subsample;
    bool _overrideThickness;
    bool _taperThickness;

    uint32 _curveCount;
    uint32 _nodeCount;

    std::vector<uint32> _curveEnds;
    std::vector<Vec4f> _nodeData;
    std::vector<Vec3f> _nodeColor;
    std::vector<Vec3f> _nodeNormals;

    std::shared_ptr<Bsdf> _bsdf;
    std::shared_ptr<TriangleMesh> _proxy;

    Box3f _bounds;

    std::unique_ptr<Bvh::BinaryBvh> _bvh;

    void loadCurves();
    void computeBounds();
    void buildProxy();

    template<bool isRibbon>
    bool intersectTemplate(Ray &ray, IntersectionTemporary &data) const;

public:
    virtual ~Curves() {}

    Curves();
    Curves(const Curves &o);
    Curves(std::vector<uint32> curveEnds, std::vector<Vec4f> nodeData, std::shared_ptr<Bsdf> bsdf, std::string name);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;
    virtual void saveResources() override;
    void saveAs(const Path &path);

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

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

    virtual Primitive *clone() override;
    
    const PathPtr& path() const
    {
        return _path;
    }
};

}

#endif /* CURVES_HPP_ */
