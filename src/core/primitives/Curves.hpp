#ifndef CURVES_HPP_
#define CURVES_HPP_

#include "Primitive.hpp"

#include "bvh/BinaryBvh.hpp"

#include <memory>
#include <vector>

namespace Tungsten {

struct CurveIntersection;
class Scene;

class Curves : public Primitive
{
    enum CurveMode
    {
        MODE_HALF_CYLINDER,
        MODE_BCSDF_CYLINDER,
        MODE_CYLINDER,
        MODE_RIBBON
    };

    std::string _path;
    std::string _dir;
    std::string _modeString;

    CurveMode _mode;

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

    void init();
    void loadCurves();
    void computeBounds();
    void buildProxy();

    template<bool isRibbon>
    bool intersectTemplate(Ray &ray, IntersectionTemporary &data) const;

public:
    virtual ~Curves() {}

    Curves();
    Curves(const Curves &o);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable() override;
    virtual float inboundPdf(const IntersectionTemporary &data, const Vec3f &p, const Vec3f &d) const override;
    virtual bool sampleInboundDirection(LightSample &sample) const override;
    virtual bool sampleOutboundDirection(LightSample &sample) const override;
    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDelta() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(const Vec3f &p) const override;
    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual void prepareForRender() override;
    virtual void cleanupAfterRender() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;

    virtual Primitive *clone() override;

    virtual void saveData() override;
};

}

#endif /* CURVES_HPP_ */
