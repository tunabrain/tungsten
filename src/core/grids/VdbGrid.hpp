#ifndef VDBGRID_HPP_
#define VDBGRID_HPP_

#if OPENVDB_AVAILABLE

#include "Grid.hpp"

#include "io/FileUtils.hpp"

#include <openvdb/openvdb.h>

namespace Tungsten {

class VdbGrid : public Grid
{
    enum class IntegrationMethod
    {
        ExactNearest,
        ExactLinear,
        Raymarching,
        ResidualRatio,
    };
    enum class SampleMethod
    {
        ExactNearest,
        ExactLinear,
        Raymarching,
    };

    typedef openvdb::tree::Tree4<openvdb::Vec2s, 5, 4, 3>::Type Vec2fTree;
    typedef openvdb::Grid<Vec2fTree> Vec2fGrid;

    PathPtr _path;
    std::string _densityName;
    std::string _emissionName;
    std::string _integrationString;
    std::string _sampleString;
    float _stepSize;
    float _densityScale;
    float _emissionScale;
    bool _scaleEmissionByDensity;
    bool _normalizeSize;
    int _supergridSubsample;
    Mat4f _configTransform;
    Mat4f _invConfigTransform;

    Vec3f _emissionIndexOffset;

    IntegrationMethod _integrationMethod;
    SampleMethod _sampleMethod;
    openvdb::FloatGrid::Ptr _densityGrid;
    openvdb::Vec3fGrid::Ptr _emissionGrid;
    Vec2fGrid::Ptr _superGrid;
    Mat4f _transform;
    Mat4f _invTransform;
    Box3f _bounds;

    static std::string sampleMethodToString(SampleMethod method);
    static std::string integrationMethodToString(IntegrationMethod method);

    static SampleMethod stringToSampleMethod(const std::string &name);
    static IntegrationMethod stringToIntegrationMethod(const std::string &name);

    void generateSuperGrid();

public:
    VdbGrid();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual Mat4f naturalTransform() const override;
    virtual Mat4f invNaturalTransform() const override;
    virtual Box3f bounds() const override;

    float density(Vec3f p) const override;
    Vec3f emission(Vec3f p) const override;
    float opticalDepth(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1) const override;
    Vec2f inverseOpticalDepth(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1, float tau) const override;
};

}

#endif

#endif /* VDBGRID_HPP_ */
